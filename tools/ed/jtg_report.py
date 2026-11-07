"""jtg_report.py RUN [BASE] [EVERY]: for a run made by tools/ed/jtg.sh. Prints the frame both players got control
(first frame in a room with nothing busy: no cutscene, menu, close-up hold or fade), each player's place then, and both
players' places every EVERY frames (default 30) from BASE on, with how far each moved since the line before.
Also `--wait RUN SECONDS`: returns (exit 0) once that control frame is in the recording, printing it; exit 1 on time out."""
import csv
import math
import os
import sys
import time


def rows(run):
    path = os.path.join(run, 'both.tsv')
    if not os.path.exists(path):
        return []
    out = []
    with open(path, newline='', encoding='utf8', errors='replace') as f:
        r = csv.reader(f, delimiter='\t')
        try:
            head = next(r)
        except StopIteration:
            return []
        for line in r:
            if len(line) != len(head):
                continue
            out.append(dict(zip(head, line)))
    return out


def num(v):
    try:
        return int(v, 0) if v.startswith('0x') else float(v)
    except ValueError:
        return float('nan')


def in_control(r):
    return (num(r['room']) >= 0 and num(r['busy']) == 0 and num(r['hold']) == 0 and num(r['cutscene']) == 0
            and num(r['menu']) == 0 and num(r['fade']) == 0 and num(r['p1st']) >= 0 and num(r['p2st']) >= 0
            and num(r['p2m']) >= 0)


def control_frame(rs):
    for r in rs:
        if in_control(r):
            return r
    return None


def place(r, p):
    return tuple(num(r[p + k]) for k in 'xyz')


def intro(run, seconds):
    """0 once ED_SKIP_INTRO saw the intro end by itself; 1 when its A taps were cut short by a press (or never ended): the
    taps can then have landed on the title menu's Start Game, a new game instead of Jump to Game."""
    begin = time.time()
    while time.time() - begin < seconds:
        for page in sorted(p for p in os.listdir(run) if p.startswith('page-')):
            with open(os.path.join(run, page), encoding='utf8', errors='replace') as f:
                for line in f:
                    if 'opening movie over' in line:
                        print(line.strip())
                        return 0
                    if 'skip-intro off at frame' in line:
                        print(line.strip())
                        return 1
        time.sleep(0.5)
    print('the intro never ended within %.0f s' % seconds)
    return 1


def main():
    if len(sys.argv) >= 2 and sys.argv[1] == '--intro':
        return intro(sys.argv[2], float(sys.argv[3]))
    if len(sys.argv) >= 2 and sys.argv[1] == '--wait':
        run, seconds = sys.argv[2], float(sys.argv[3])
        begin = time.time()
        size, since = -1, time.time()
        path = os.path.join(run, 'both.tsv')
        while time.time() - begin < seconds:
            c = control_frame(rows(run))
            if c is not None:
                print(c['frame'])
                return 0
            now = os.path.getsize(path) if os.path.exists(path) else -1
            if now != size:
                size, since = now, time.time()
            elif time.time() - since > 10:
                return 1  # the game closed (or froze) before control
            time.sleep(1.0)
        return 1
    run = sys.argv[1]
    base = int(sys.argv[2]) if len(sys.argv) > 2 else None
    every = int(sys.argv[3]) if len(sys.argv) > 3 else 30
    rs = rows(run)
    if not rs:
        print('no recording in', run)
        return 1
    c = control_frame(rs)
    if c is None:
        print('control never came (last frame %s)' % rs[-1]['frame'])
        return 1
    print('control at frame %s, room %s: P1 (model %s) at %.0f %.0f %.0f, P2 (model %s) at %.0f %.0f %.0f' % (
        c['frame'], c['room'], c['p1m'], *place(c, 'p1'), c['p2m'], *place(c, 'p2')))
    if base is not None:
        print('base frame %d: control came %+d frames from it%s' % (
            base, int(c['frame']) - base, '  << LATE: presses before it were lost' if int(c['frame']) > base else ''))
    start = base if base is not None else int(c['frame'])
    print('frame  room   P1 x y z            moved  stick    P2 x y z            moved  stick   busy')
    was = None
    for r in rs:
        f = int(r['frame'])
        if f < start or (f - start) % every:
            continue
        a, b = place(r, 'p1'), place(r, 'p2')
        m1 = math.dist(a, was[0]) if was else 0.0
        m2 = math.dist(b, was[1]) if was else 0.0
        print('%5d  %4s  %6.0f %6.0f %5.0f  %6.0f  %3s,%-4s %6.0f %6.0f %5.0f  %6.0f  %3s,%-4s %s' % (
            f, r['room'], *a, m1, r['p1sx'], r['p1sy'], *b, m2, r['p2sx'], r['p2sy'], r['busy']))
        was = (a, b)
    last = rs[-1]
    print('last frame %s: P1 at %.0f %.0f %.0f, P2 at %.0f %.0f %.0f (room %s)' % (
        last['frame'], *place(last, 'p1'), *place(last, 'p2'), last['room']))
    return 0


if __name__ == '__main__':
    sys.exit(main())
