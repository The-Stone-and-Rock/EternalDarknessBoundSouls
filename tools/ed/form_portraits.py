"""form_portraits.py [NAME...]: a picture of each character's other form for the character screens, by photo mode.
Each form walks to where slot 5's room camera is close and faces it, in a run on a copy of the settings. Photo mode takes
their head and shoulders (ED_PHOTO_PORTRAIT), and it's cut to the size of the portraits the menu writes from the disc. They go
in res/ui/portraits/ beside those, as NAME-FORM.png, and aren't committed, as those aren't. Closes any running copy of the
game. A form with no picture shows the normal one. The photos stay in build/portraits, to look at."""
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, os.path.dirname(__file__))
import chapters

# name: Mods' place, form, how high their head is, how far to stand from it
FORMS = {
    'Pius-1': (1, 1, 225, 130),
    'Ellia-1': (2, 1, 195, 110),
    'Ellia-2': (2, 2, 195, 110),
    'Anthony-1': (3, 1, 215, 120),
    'Anthony-2': (3, 2, 215, 120),
    'Karim-1': (4, 1, 215, 120),
    'Max-1': (5, 1, 200, 130),
    'Paul-1': (7, 1, 205, 120),
    'Paul-2': (7, 2, 205, 120),
    'Paul-3': (7, 3, 205, 120),
    'Paul-4': (7, 4, 205, 120),
    'Roberto-1': (8, 1, 205, 120),
    'Edward-1': (10, 1, 215, 120),
}
SLOT = 5
AFTER = 1217  # frames from the save: after the walk's first turn, near the camera and facing it
OUT = (126, 89)


def take(name, character, form, height, distance):
    text = open(os.path.join(ROOT, 'settings.ini'), encoding='utf-8', errors='replace').read()
    for key, value in (('mod_coop', 0), ('mod_p1_character', character), ('mod_p1_costume', form << ((character - 1) * 4)), ('mod_p1_stats', 1)):
        text = re.sub(r'(?m)^%s = .*$' % key, '%s = %s' % (key, value), text)
    out = os.path.join(ROOT, 'build', 'portraits')
    os.makedirs(out, exist_ok=True)
    copy = os.path.join(out, 'settings.ini')
    open(copy, 'w', encoding='utf-8').write(text)
    start = chapters.first_frame(SLOT)
    at = start + 900
    presses = []
    for x, y in ((0, 127), (127, 0), (0, -127)):
        presses.append('%d,%d,0x0000,%d,%d,0' % (at, at + 90, x, y))
        at += 130
    photo = os.path.join(out, name + '.png')
    if os.path.exists(photo):
        os.remove(photo)
    env = dict(os.environ, ED_SETTINGS=copy, ED_INPUT_SCRIPT=';'.join(presses), ED_RUN_SHOTS_MS='5000', ED_RUN_SECONDS='70',
               ED_PHOTO_PORTRAIT='%d,%g,%g,%s' % (start + AFTER, height, distance, photo.replace('\\', '/')))
    sh = shutil.which('sh') or r'C:\Program Files\Git\bin\sh.exe'
    subprocess.run([sh, 'tools/ed/run.sh', 'saves/slot%d.state' % SLOT, 'portrait-' + name], cwd=ROOT, env=env, capture_output=True, text=True, errors='replace')
    return photo


def cut(source, target):
    """The middle of the photo, at the portraits' shape, at their size."""
    script = ('Add-Type -AssemblyName System.Drawing;'
              '$s=[System.Drawing.Image]::FromFile("%s");'
              '$h=$s.Height;$w=[int]($h*%d/%d);$x=[int](($s.Width-$w)/2);'
              '$d=New-Object System.Drawing.Bitmap %d,%d;'
              '$g=[System.Drawing.Graphics]::FromImage($d);'
              '$g.InterpolationMode="HighQualityBicubic";'
              '$g.DrawImage($s,(New-Object System.Drawing.Rectangle 0,0,%d,%d),(New-Object System.Drawing.Rectangle $x,0,$w,$h),[System.Drawing.GraphicsUnit]::Pixel);'
              '$d.Save("%s",[System.Drawing.Imaging.ImageFormat]::Png);$s.Dispose();$d.Dispose()'
              % (source, OUT[0], OUT[1], OUT[0], OUT[1], OUT[0], OUT[1], target))
    subprocess.run(['powershell', '-NoProfile', '-Command', script], check=True)


def main():
    for name in sys.argv[1:] or list(FORMS):
        photo = take(name, *FORMS[name])
        if not os.path.exists(photo):
            print(name, 'no photo')
            continue
        cut(photo, os.path.join(ROOT, 'res', 'ui', 'portraits', name + '.png'))
        print(name, photo, flush=True)


if __name__ == '__main__':
    main()
