"""`ed zoom RUN FRAME [X Y W H] [--scale S] [--name NAME]`: one picture of a run at a game frame, a part of it enlarged,
saved in "Screenshots for Claude/zoom/" (the folder CLAUDE.md asks for), to look at a detail closely: a Tome page's
plaques, a prompt, a half. RUN is a run folder or the end of its name (as `ed stops`); the picture is the run's shot
taken at or after FRAME (both.tsv gives the frame's time, shots/shots.csv each shot's). Without X Y W H, the whole shot.
"""
import csv
import glob
import os
import subprocess

import disc

OUT = os.path.join(disc.ROOT, 'Screenshots for Claude', 'zoom')


def run_dir(name):
    if os.path.isdir(name):
        return name
    found = sorted(glob.glob(os.path.join(disc.ROOT, 'logs', 'run-*' + name + '*')))
    if not found:
        raise SystemExit('ed zoom: no run named %s' % name)
    return found[-1]


def shot_at(run, frame):
    """The shot file taken at or after the frame's time, and that time (ms)."""
    at = None
    with open(os.path.join(run, 'both.tsv'), encoding='utf8', errors='replace') as f:
        rows = csv.reader(f, delimiter='\t')
        head = next(rows)
        fi, mi = head.index('frame'), head.index('ms')
        for r in rows:
            if int(r[fi]) >= frame:
                at = int(r[mi])
                break
    if at is None:
        raise SystemExit('ed zoom: frame %d is past the run' % frame)
    with open(os.path.join(run, 'shots', 'shots.csv'), encoding='utf8') as f:
        for r in csv.DictReader(f):
            if int(r['ms']) >= at:
                return os.path.join(run, 'shots', r['file'])
    raise SystemExit('ed zoom: no shot at or after frame %d' % frame)


def cmd_zoom(argv):
    if len(argv) < 2:
        raise SystemExit(__doc__)
    scale, name = 3.0, None
    if '--scale' in argv:
        i = argv.index('--scale')
        scale = float(argv[i + 1])
        del argv[i:i + 2]
    if '--name' in argv:
        i = argv.index('--name')
        name = argv[i + 1]
        del argv[i:i + 2]
    run = run_dir(argv[0])
    frame = int(argv[1])
    src = shot_at(run, frame)
    if len(argv) >= 6:
        x, y, w, h = (int(v) for v in argv[2:6])
    else:
        x, y, w, h, scale = 0, 0, 1280, 720, 1.0 if '--scale' not in argv else scale
    os.makedirs(OUT, exist_ok=True)
    out = os.path.join(OUT, '%s-%d%s.png' % (os.path.basename(run).replace('run-', ''), frame, '-' + name if name else ''))
    subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                    os.path.join(os.path.dirname(os.path.abspath(__file__)), 'zoom.ps1'), '-Src', src, '-X', str(x),
                    '-Y', str(y), '-W', str(w), '-H', str(h), '-Scale', str(scale), '-Out', out], check=True,
                   stdout=subprocess.DEVNULL)
    print(out.replace('\\', '/'))
