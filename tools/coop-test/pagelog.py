# Reads the game's output and writes it as numbered pages with headers, an index
# of the pages, and one file of the events worth reading first.
import sys, os, re, time
root = sys.argv[1]; PAGE = int(sys.argv[2]) if len(sys.argv) > 2 else 20000
os.makedirs(root, exist_ok=True)
KEY = re.compile(r'^\[(sound\]|dtk\]|stay\]|room\] (ticket|change)|controls\] cutscene|frontend\]|quick\]|run\]|split\] (on|off)|player\]|cheat\])|^\[coop\] (script|player 2 (brought|arrived|opened|has|opens|fell|stood|\(listed\))|player 1 has|a cutscene|the cutscene over|play goes on|a script asks|[0-9]+ (saw|seen|goes)|[0-9]+.s health|.* not found)|rror|xception|not found|crash|hang|stuck', re.I)
FRAME = re.compile(r'\(frame (\d+)\)|at frame (\d+)')
ROOM = re.compile(r'\[room\] change 0x([0-9A-Fa-f]+)')
now = lambda: time.strftime('%Y-%m-%d %H:%M:%S')
idx = open(os.path.join(root, 'INDEX.txt'), 'a', encoding='utf8', buffering=1)
ev = open(os.path.join(root, 'EVENTS.log'), 'a', encoding='utf8', buffering=1)
idx.write('Started %s. %d lines a page. EVENTS.log lists the key lines as page:line.\n' % (now(), PAGE))
n = 0; f = None; line_no = 0; first = last = None; rooms = []; events = 0; started = ''
def close(note):
    global f
    if f is None: return
    f.write('=== END OF PAGE %04d | %s | %s | frames %s-%s | rooms %s | %d events ===\n' % (n, now(), note, first, last, ' '.join(rooms) or '-', events))
    f.close(); f = None
    idx.write('page-%04d.log | %s to %s | %d lines | frames %s-%s | rooms %s | %d events | %s\n' % (n, started, now(), line_no, first, last, ' '.join(rooms) or '-', events, note))
def open_page():
    global f, n, line_no, first, last, rooms, events, started
    n += 1; line_no = 0; first = last = None; rooms = []; events = 0; started = now()
    f = open(os.path.join(root, 'page-%04d.log' % n), 'w', encoding='utf8', buffering=1)
    f.write('=== PAGE %04d | started %s | previous page-%04d.log | see INDEX.txt, EVENTS.log ===\n' % (n, started, n - 1))
open_page()
for raw in sys.stdin.buffer:
    s = raw.decode('utf8', 'replace').rstrip('\r\n')
    if line_no >= PAGE:
        close('continues in page-%04d.log' % (n + 1)); open_page()
    line_no += 1
    f.write(s + '\n')
    m = FRAME.search(s)
    if m:
        fr = m.group(1) or m.group(2); first = first or fr; last = fr
    m = ROOM.search(s)
    if m: rooms.append(m.group(1).lstrip('0') or '0')
    if KEY.search(s):
        events += 1; ev.write('%04d:%05d %s %s\n' % (n, line_no + 1, time.strftime('%H:%M:%S'), s[:200].split(' calls:')[0]))
close('game closed')
idx.write('Closed %s.\n' % now())
