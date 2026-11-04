"""What a door did in a run, frame by frame, from both.tsv: python -I tools/ed/facts/checks/watch_door.py RUN [--pad N]
Prints the frames where the room, state, motion or busy word of either player changes (with the countdown and fade then), and the numbers that answer
what static reading could not: frames per tick, and whether the door's motion is still playing at the room change."""
import csv
import glob
import os
import sys

run = sys.argv[1]
if not os.path.isdir(run):
    hits = sorted(glob.glob('logs/' + run)) or sorted(glob.glob('logs/run-2*-' + run))
    if not hits:
        raise SystemExit('no run named %s under logs/' % run)
    run = hits[-1]
if not os.path.exists(os.path.join(run, 'both.tsv')):
    raise SystemExit('%s has no both.tsv: not made with tools/ed/run.sh' % run)
rows = list(csv.DictReader(open(os.path.join(run, 'both.tsv'), encoding='utf8', errors='replace'), delimiter='\t'))
if rows and 'ticket' not in rows[0]:
    raise SystemExit('%s is older than the ticket column: run it again' % run)
rows = [r for r in rows if r.get('p2anim') not in (None, '')]
KEYS = ('room', 'entry', 'p1st', 'p1anim', 'p2st', 'p2anim', 'busy', 'script')
was = None
start = None
for r in rows:
    if start is None and int(r['ticket']) > 1:
        start = r
    now = tuple(r[k] for k in KEYS)
    if now != was and start is not None:
        print('%s %6sms ticket %-4s fade %-4s room %-5s p1 st %-4s anim %-4s at %8s %8s %8s | p2 st %-4s anim %-4s at %8s %8s %8s | busy %s eye %s %s %s fov %s' % (
            r['frame'], int(r['ms']) - int(start['ms']), r['ticket'], r['fade'], r['room'], r['p1st'], r['p1anim'], r['p1x'], r['p1y'], r['p1z'], r['p2st'], r['p2anim'], r['p2x'], r['p2y'], r['p2z'], r['busy'], r['ex'], r['ey'], r['ez'], r['fov']))
    was = now
if start is None:
    raise SystemExit('no room change asked for in this run')
first_room = start['room']
change = next((r for r in rows if int(r['frame']) > int(start['frame']) and r['room'] != first_room), None)
if change:
    n = int(change['frame']) - int(start['frame'])
    ms = int(change['ms']) - int(start['ms'])
    print('\nFirst ticket %s at frame %s, room %s to %s at frame %s: %d frames, %d ms' % (start['ticket'], start['frame'], first_room, change['room'], change['frame'], n, ms))
    print('Ticks per frame: %.2f (ticket %s counted in %d frames)' % (int(start['ticket']) / n, start['ticket'], n))
    before = rows[rows.index(change) - 1]
    print('At the room change: player 1 state %s motion %s (just before: state %s motion %s), player 2 state %s motion %s' % (change['p1st'], change['p1anim'], before['p1st'], before['p1anim'], change['p2st'], change['p2anim']))
    after = [r for r in rows if int(r['frame']) >= int(change['frame'])][:900]
    seq = []
    for r in after:
        t = (r['p1st'], r['p1anim'])
        if not seq or seq[-1][1] != t:
            seq.append((r['frame'], t))
    print('Player 1 after arrival, (frame, (state, motion)):', seq[:10])
    seq = []
    for r in after:
        t = (r['p2st'], r['p2anim'])
        if not seq or seq[-1][1] != t:
            seq.append((r['frame'], t))
    print('Player 2 after arrival:', seq[:10])
