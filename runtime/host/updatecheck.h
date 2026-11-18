// The launcher's Check Updates button: asks GitHub for the project's releases on a thread of its own, so the launcher
// never waits for the network. Nothing is downloaded or installed: the launcher only opens the Releases page.
#pragma once

#include <string>

enum UpdateState { kUpdateIdle, kUpdateChecking, kUpdateNone, kUpdateNewer, kUpdateFailed };

// Start a check against this build's version ("0.1.6"). Does nothing while one is running.
void updatecheck_start(const std::string& current);
UpdateState updatecheck_state();
// The newest release's version ("0.1.7") once the state is kUpdateNewer or kUpdateNone.
std::string updatecheck_latest();
// The page the Update button opens.
const char* updatecheck_page();
