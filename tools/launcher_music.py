#!/usr/bin/env python3
"""The launcher's music, from your own game: the song of its Options screen.

That song is MusyX music (played from note data, not a recording on the
disc), so it is taken from the game's own sound: record it, then cut one
loop (64.009 s) into res/music/launcher.wav (git ignores it), which the
launcher plays in a loop.

1. Record: run the game with ED_PCM_OUT=options.pcm, open the title menu's
   Options, leave it untouched for over two minutes, quit.
2. Cut:    python tools/launcher_music.py options.pcm SECONDS
   SECONDS: a time in the recording well inside the Options music (a few
   seconds after the screen opened).
"""
import os
import struct
import sys

RATE = 32000            # the game's sound output, s16 stereo
LOOP = 2048297          # the song's length in samples (measured)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    data = open(sys.argv[1], "rb").read()
    start = int(float(sys.argv[2]) * RATE) * 4
    loop = data[start:start + LOOP * 4]
    if len(loop) < LOOP * 4:
        sys.exit("the recording is too short past that time: record longer")
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = os.path.join(root, "res", "music", "launcher.wav")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(loop)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 2, RATE, RATE * 4, 4, 16))
        f.write(b"data" + struct.pack("<I", len(loop)) + loop)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
