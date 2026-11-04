"""The graphics options and character shadows, measured from inside the renderer.

`ed flicker RUN` reads a run's probe (run.sh with ED_RUN_PROBE=1): a line for each player's view of each moment drawn, with what
fed its shadows and a small grid of what they darkened and the picture after them. So a half can be measured on its own, frame by
frame, with no window capture. A spike is a view that jumps and comes straight back.

`ed lamps RUN [RUN...]` lists the lamps each run was seen to have, from the log's [shadow-lamp] lines (AURORA_SHADOW_LOG).

`ed graphics [SAVE] [OPTION...]` runs the game once for every value of every graphics option, from a copy of settings.ini with
only that one changed, and says what each did to the picture, the frame rate and the log. An option that changes nothing, or an
error, is a finding. Nobody should hold a controller while it runs.
"""
import glob
import os
import re
import shutil
import subprocess
import sys

import recording

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# name: the values to try that differ from a default. Every value of each is tried except the one settings.ini has.
OPTIONS = {
    'frame_rate': [60, 120],
    'anti_aliasing': [0, 1],
    'glow': [0, 1, 2],
    'hdr': [0, 1],
    'ssao': [0, 1, 2],
    'ssao_high': [0, 1],
    'smaa': [0, 1],
    'sharpen': [0, 1, 2, 3],
    'shafts': [0, 1, 2],
    'per_pixel_lighting': [0, 1],
    'char_shadows': [0, 1, 2],
    'shadow_hard': [0, 1],
    'contact_shadows': [0, 1],
    'volumetric': [0, 1, 2],
    'skin_scatter': [0, 1],
    'spell_rim': [0, 1],
    'sharp_shadows': [0, 1],
    'hd_textures': [0, 1],
}


def load_probe(path):
    rows = []
    for line in open(path):
        if ' grid ' not in line:
            continue
        head, grid = line.rstrip('\n').split(' grid ')
        w = head.split()
        d = dict(seq=int(w[1]), frame=int(w[3]), kind=int(w[5]), view=int(w[7]), tiles=int(w[11]), vol=int(w[13]))
        if grid == '-':
            d['mask'] = d['col'] = None
        else:
            b = bytes.fromhex(grid)
            d['mask'] = b[0::2]
            d['col'] = b[1::2]
        rows.append(d)
    rows.sort(key=lambda r: r['seq'])
    return rows


def diff(a, b):
    return sum(abs(x - y) for x, y in zip(a, b)) / max(len(a), 1)


def spikes(series, name, threshold):
    out = []
    for i in range(1, len(series) - 1):
        a, b, c = series[i - 1][name], series[i][name], series[i + 1][name]
        d1, d2, d0 = diff(a, b), diff(b, c), diff(a, c)
        if d1 > threshold and d2 > threshold and d0 < 0.5 * min(d1, d2):
            out.append((series[i]['frame'], series[i]['kind'], round(d1, 2), round(d2, 2)))
    return out


def summary(path, threshold=0.1):
    rows = load_probe(path)
    res = {'renders': len(rows), 'between': sum(r['kind'] == 2 for r in rows), 'views': {}}
    for view in (0, 1):
        series = [r for r in rows if r['view'] == view and r['mask'] is not None]
        if len(series) < 3:
            continue
        v = {}
        for name in ('mask', 'col'):
            steps = sorted(diff(series[i - 1][name], series[i][name]) for i in range(1, len(series)))
            v[name] = dict(median=steps[len(steps) // 2], p99=steps[int(len(steps) * 0.99)], max=steps[-1], spikes=spikes(series, name, threshold))
        v['tile_drops'] = [series[i]['frame'] for i in range(1, len(series) - 1) if series[i]['tiles'] < series[i - 1]['tiles'] - 1 and series[i + 1]['tiles'] > series[i]['tiles'] + 1]
        res['views'][view] = v
    return res


def cmd_flicker(argv):
    threshold = 0.1
    args = []
    for a in argv:
        if a.startswith('--threshold='):
            threshold = float(a.split('=')[1])
        else:
            args.append(a)
    path = args[0] if args and os.path.isfile(args[0]) else os.path.join(recording.find_run(args[0] if args else None), 'probe.txt')
    if not os.path.exists(path):
        raise SystemExit('no probe at %s. Run with ED_RUN_PROBE=1' % path)
    s = summary(path, threshold)
    print('%s: %d views drawn, %d of them moments between frames' % (path, s['renders'], s['between']))
    for view, v in sorted(s['views'].items()):
        for name, label in (('mask', 'shadows'), ('col', 'picture')):
            m = v[name]
            print('player %d %s: step median %.3f, 99%% %.3f, largest %.3f, spikes %d' % (view + 1, label, m['median'], m['p99'], m['max'], len(m['spikes'])))
            for frame, kind, d1, d2 in m['spikes'][:20]:
                print('   frame %d (%s) %.2f then %.2f' % (frame, 'between' if kind == 2 else 'its own', d1, d2))
        if v['tile_drops']:
            print('player %d shadows dropping out for one frame at: %s' % (view + 1, v['tile_drops'][:20]))


def cmd_lamps(argv):
    runs = [recording.find_run(a) for a in argv] or [recording.find_run(None)]
    for run in runs:
        rows = recording.load(run)
        print('%s, room %s' % (os.path.basename(run), rows[0].get('room', '?') if rows else '?'))
        for path in sorted(glob.glob(os.path.join(run, 'page-*.log'))):
            for line in open(path, errors='replace'):
                m = re.search(r'\[shadow-lamp\] (.*)', line)
                if m:
                    print('  ' + m.group(1))


def picture_stats(run):
    """The last few pictures' mean as a small grey image, per half, and how much fine detail one holds."""
    files = sorted(glob.glob(os.path.join(run, 'shots', '*.jpg')))[-4:]
    if not files:
        return None
    tot = None
    used = 0
    for f in files:
        got = subprocess.run(['ffmpeg', '-v', 'error', '-i', f, '-vf', 'scale=64:18,format=gray', '-f', 'rawvideo', '-'], capture_output=True).stdout
        if len(got) != 64 * 18:
            continue
        used += 1
        tot = list(got) if tot is None else [a + b for a, b in zip(tot, got)]
    return [x / used for x in tot] if used else None


def detail_of(run):
    """Mean step between neighbouring pixels at full size, for what's too fine to change the small picture: SMAA, sharpening, MSAA."""
    files = sorted(glob.glob(os.path.join(run, 'shots', '*.jpg')))[-3:]
    total = n = 0
    for f in files:
        w, h = 960, 540
        got = subprocess.run(['ffmpeg', '-v', 'error', '-i', f, '-vf', 'crop=iw/2:ih/2:iw/4:ih/4,scale=%d:%d:flags=neighbor,format=gray' % (w, h), '-f', 'rawvideo', '-'], capture_output=True).stdout
        if len(got) != w * h:
            continue
        for y in range(0, h, 3):
            row = got[y * w:(y + 1) * w]
            total += sum(abs(row[i] - row[i + 1]) for i in range(w - 1))
            n += w - 1
    return total / n if n else 0.0


def halves(a, b):
    left = right = 0.0
    for y in range(18):
        for x in range(64):
            d = abs(a[y * 64 + x] - b[y * 64 + x])
            if x < 32:
                left += d
            else:
                right += d
    return left / (18 * 32), right / (18 * 32)


def read_ini(path):
    out = {}
    for line in open(path, encoding='utf8', errors='replace'):
        if ' = ' in line:
            k, v = line.rstrip('\n').split(' = ', 1)
            out[k] = v
    return out


def run_one(save, name, settings_path, frames, seconds=70):
    env = dict(os.environ)
    env.update(ED_SETTINGS=settings_path, ED_RUN_FRAMES=str(frames), ED_RUN_SECONDS=str(seconds), ED_RUN_PROBE='1', DOL_INTERP_LOG='1', ED_RUN_SHOTS_W='2560')
    sh = shutil.which('sh') or r'C:\Program Files\Git\bin\sh.exe'
    out = subprocess.run([sh, 'tools/ed/run.sh', save, name], cwd=ROOT, env=env, capture_output=True, text=True, errors='replace').stdout
    return os.path.join(ROOT, out.split('\n')[0].strip())


def log_text(run):
    text = ''
    for path in sorted(glob.glob(os.path.join(run, 'page-*.log'))):
        text += open(path, errors='replace').read()
    return text


def interp_stats(text):
    lines = re.findall(r'\[interp\] screen=([\d.]+)Hz game=([\d.]+)Hz shown=(\d+)/(\d+) frames blended=(\d+) still=(\d+) cut=(\d+) none=(\d+) caught_up=(\d+)', text)[2:]
    if not lines:
        return None
    shown = sum(int(l[2]) for l in lines)
    frames = sum(int(l[3]) for l in lines)
    caught = sum(int(l[8]) for l in lines)
    return dict(screen=float(lines[-1][0]), shown=shown / len(lines), frames=frames / len(lines), caught=caught / max(frames, 1))


def cmd_graphics(argv):
    save = 'saves/slot10.state'
    only = []
    frames = None
    for a in argv:
        if a.endswith('.state'):
            save = a
        elif a.startswith('--frames='):
            frames = int(a.split('=')[1])
        else:
            only.append(a)
    base_ini = os.path.join(ROOT, 'settings.ini')
    base = read_ini(base_ini)
    out_dir = os.path.join(ROOT, 'build', 'runs', 'graphics')
    os.makedirs(out_dir, exist_ok=True)

    def settings_with(key=None, value=None):
        path = os.path.join(out_dir, 'settings.ini')
        lines = []
        for line in open(base_ini, encoding='utf8', errors='replace'):
            if key is not None and line.startswith(key + ' = '):
                line = '%s = %s\n' % (key, value)
            lines.append(line)
        open(path, 'w', encoding='utf8').writelines(lines)
        return path

    # When to stop: long enough for a camera's first pan to settle, from the save's own first frame.
    def first_frame(run):
        rows = recording.load(run)
        return int(rows[0]['frame']), int(rows[-1]['frame'])

    print('save %s. Everything as settings.ini has it, then one thing changed at a time.' % save)
    probe = run_one(save, 'gfx-base', settings_with(), 10 ** 9 if frames is None else frames, seconds=14) if frames is None else run_one(save, 'gfx-base', settings_with(), frames)
    start, end = first_frame(probe)
    frames = frames or end
    base_pic = picture_stats(probe)
    text = log_text(probe)

    base_detail = detail_of(probe)
    noise = [0.0, 0.0]

    def report(label, run, text):
        s = summary(os.path.join(run, 'probe.txt')) if os.path.exists(os.path.join(run, 'probe.txt')) else None
        pic = picture_stats(run)
        changed = halves(base_pic, pic) if pic and base_pic else (0.0, 0.0)
        detail = detail_of(run)
        d = 100.0 * (detail - base_detail) / base_detail if base_detail else 0.0
        i = interp_stats(text)
        rate = '%.0f of %.0f Hz' % (i['shown'], i['screen']) if i else 'no frames between'
        errs = len(re.findall(r'WebGPU error|\[wgpu\].*error|hang', text))
        drops = sum(len(v['tile_drops']) for v in s['views'].values()) if s else 0
        spk = ' '.join('p%d %d' % (v + 1, len(s['views'][v]['col']['spikes'])) for v in sorted(s['views'])) if s and s['views'] else '-'
        print('%-24s picture changed %5.2f %5.2f  detail %+6.1f%%  %-18s spikes %-9s drops %d errors %d  %s' % (label, changed[0], changed[1], d, rate, spk, drops, errs, os.path.basename(run)))
        sys.stdout.flush()
        return max(changed), abs(d), errs, drops

    report('as settings.ini', probe, text)
    # The same again, for what a run differs from itself by: the lamps flicker.
    run = run_one(save, 'gfx-base2', settings_with(), frames)
    noise = report('again, the same', run, log_text(run))
    print('A change under about %.2f, or %.1f%% of detail, can\'t be told from the lamps flickering.' % (noise[0] * 1.5, noise[1] * 1.5))
    findings = []
    quiet = []
    for key, values in OPTIONS.items():
        if only and key not in only:
            continue
        for value in values:
            if key not in base or str(value) == base[key]:
                continue
            run = run_one(save, 'gfx-%s-%s' % (key, value), settings_with(key, value), frames)
            changed, d, errs, drops = report('%s = %s' % (key, value), run, log_text(run))
            if errs:
                findings.append('%s = %s: errors in the log' % (key, value))
            if drops:
                findings.append('%s = %s: shadows dropped out for a frame' % (key, value))
            if key != 'frame_rate' and changed <= noise[0] * 1.5 and d <= noise[1] * 1.5:
                quiet.append('%s = %s' % (key, value))
    print('\n%d finding(s)' % len(findings))
    for f in findings:
        print('  ' + f)
    print('No change to be seen in this room and light (not always a fault: a spell has to be cast for spell_rim): ' + (', '.join(quiet) or 'none'))
