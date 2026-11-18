// The launcher's Check Updates button (updatecheck.h). One request to GitHub's public list of the project's releases,
// on a detached thread with short timeouts: a slow or missing network only ends in "could not check". The "latest
// release" address is not used: it skips pre-releases, and every release of this project is one, so the list is read
// and the highest version in it is the newest.
#include "host/updatecheck.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace {

const char* const kHost = "api.github.com";
const char* const kPath = "/repos/The-Stone-and-Rock/EternalDarknessBoundSouls/releases?per_page=30";
const char* const kPage = "https://github.com/The-Stone-and-Rock/EternalDarknessBoundSouls/releases";
const char* const kAgent = "EternalDarknessBoundSouls-launcher";

std::atomic<int> g_state{kUpdateIdle};
std::mutex g_mutex;
std::string g_latest;

// "v0.1.7" or "0.1.7-beta" as numbers: {0, 1, 7}.
std::vector<int> version_numbers(const std::string& text) {
    std::vector<int> out;
    size_t i = 0;
    while (i < text.size() && (text[i] < '0' || text[i] > '9'))
        i++;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
        int n = 0;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9')
            n = n * 10 + (text[i++] - '0');
        out.push_back(n);
        if (i < text.size() && text[i] == '.')
            i++;
        else
            break;
    }
    return out;
}

bool newer(const std::vector<int>& a, const std::vector<int>& b) {
    for (size_t i = 0; i < a.size() || i < b.size(); i++) {
        const int x = i < a.size() ? a[i] : 0, y = i < b.size() ? b[i] : 0;
        if (x != y)
            return x > y;
    }
    return false;
}

std::string plain(const std::vector<int>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); i++)
        out += (i ? "." : "") + std::to_string(v[i]);
    return out;
}

// The highest version among the list's "tag_name" values; empty when there is none.
std::vector<int> highest_tag(const std::string& json) {
    std::vector<int> best;
    const std::string key = "\"tag_name\"";
    for (size_t at = json.find(key); at != std::string::npos; at = json.find(key, at + key.size())) {
        const size_t open = json.find('"', json.find(':', at + key.size()));
        if (open == std::string::npos)
            break;
        const size_t close = json.find('"', open + 1);
        if (close == std::string::npos)
            break;
        const std::vector<int> v = version_numbers(json.substr(open + 1, close - open - 1));
        if (!v.empty() && (best.empty() || newer(v, best)))
            best = v;
    }
    return best;
}

bool fetch(std::string* body) {
#ifdef _WIN32
    bool ok = false;
    const std::wstring agent(kAgent, kAgent + std::strlen(kAgent)), host(kHost, kHost + std::strlen(kHost)),
        path(kPath, kPath + std::strlen(kPath));
    HINTERNET session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr)
        return false;
    WinHttpSetTimeouts(session, 5000, 5000, 5000, 8000);
    HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = connect == nullptr ? nullptr
                                           : WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                                                WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (request != nullptr &&
        WinHttpSendRequest(request, L"Accept: application/vnd.github+json\r\n", static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0,
                           0, 0) &&
        WinHttpReceiveResponse(request, nullptr)) {
        DWORD status = 0, size = sizeof status;
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status,
                            &size, WINHTTP_NO_HEADER_INDEX);
        if (status == 200) {
            char chunk[8192];
            DWORD got = 0;
            ok = true;
            while (WinHttpReadData(request, chunk, sizeof chunk, &got) && got != 0) {
                body->append(chunk, got);
                if (body->size() > 4u * 1024u * 1024u) {  // (a list of releases is far smaller)
                    ok = false;
                    break;
                }
            }
        }
    }
    if (request != nullptr)
        WinHttpCloseHandle(request);
    if (connect != nullptr)
        WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok;
#else
    // Elsewhere: the system's curl, quiet, with the same short limits.
    const std::string command = std::string("curl -fsSL --max-time 8 -A ") + kAgent + " 'https://" + kHost + kPath + "' 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr)
        return false;
    char chunk[8192];
    size_t got;
    while ((got = fread(chunk, 1, sizeof chunk, pipe)) > 0 && body->size() < 4u * 1024u * 1024u)
        body->append(chunk, got);
    return pclose(pipe) == 0;
#endif
}

void finish(UpdateState state, const std::string& latest) {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_latest = latest;
    }
    g_state = state;
}

void check(std::string current) {
    // ED_UPDATE_TEST=0.1.7 (testing): the answer is that version, with no request; "fail": the check fails.
    if (const char* test = std::getenv("ED_UPDATE_TEST"); test != nullptr && test[0] != '\0') {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        const std::vector<int> v = version_numbers(test);
        if (v.empty())
            finish(kUpdateFailed, "");
        else
            finish(newer(v, version_numbers(current)) ? kUpdateNewer : kUpdateNone, plain(v));
        return;
    }
    std::string body;
    const bool ok = fetch(&body);
    const std::vector<int> best = ok ? highest_tag(body) : std::vector<int>();
    if (best.empty()) {
        std::fprintf(stderr, "[update] could not check (%s)\n", ok ? "no release in the answer" : "no answer");
        finish(kUpdateFailed, "");
        return;
    }
    const bool is_newer = newer(best, version_numbers(current));
    std::fprintf(stderr, "[update] this is %s, the newest release is %s%s\n", current.c_str(), plain(best).c_str(),
                 is_newer ? ": newer" : "");
    finish(is_newer ? kUpdateNewer : kUpdateNone, plain(best));
}

}  // namespace

void updatecheck_start(const std::string& current) {
    int was = g_state.load();
    do {
        if (was == kUpdateChecking)
            return;
    } while (!g_state.compare_exchange_weak(was, kUpdateChecking));
    std::thread(check, current).detach();
}

UpdateState updatecheck_state() { return static_cast<UpdateState>(g_state.load()); }

std::string updatecheck_latest() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_latest;
}

const char* updatecheck_page() { return kPage; }
