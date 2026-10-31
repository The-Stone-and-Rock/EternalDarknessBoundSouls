#!/usr/bin/env python3
"""The title menu's moving background, from a video of your own.

Mods' Title video plays it in a loop behind the title menu, in place of the
game's still picture (titlevideo.cpp); the game's own music goes on. The
video is made from the game's art, so it is not in this repository: this
converts yours into res/video/title.mpg (git ignores it), MPEG-1 for the
decoder the program carries (runtime/third_party/pl_mpeg.h), without sound,
1280x720 at 60 frames a second, with its bottom-left corner (a small label)
blurred.

    python tools/title_video.py VIDEO [FFMPEG]

FFMPEG: the ffmpeg to use, else the one on the PATH.
"""
import os
import subprocess
import sys

WIDTH, HEIGHT, RATE = 1280, 720, 60
# The corner blurred: x, y, width, height in the 1280x720 frame.
BLUR = (0, 676, 240, 44)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    ffmpeg = sys.argv[2] if len(sys.argv) > 2 else "ffmpeg"
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = os.path.join(root, "res", "video", "title.mpg")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    x, y, w, h = BLUR
    graph = (f"[0:v]scale={WIDTH}:{HEIGHT},fps={RATE},split[a][b];"
             f"[b]crop={w}:{h}:{x}:{y},boxblur=10:3:5:3[c];"
             f"[a][c]overlay={x}:{y}[v]")
    subprocess.run([ffmpeg, "-v", "error", "-y", "-i", sys.argv[1], "-filter_complex", graph,
                    "-map", "[v]", "-an", "-c:v", "mpeg1video", "-q:v", "3", "-bf", "0",
                    "-f", "mpeg", out], check=True)
    print(f"wrote {out} ({os.path.getsize(out) / 1e6:.1f} MB)")


if __name__ == "__main__":
    main()
