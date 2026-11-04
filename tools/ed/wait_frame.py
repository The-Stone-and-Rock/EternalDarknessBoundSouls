"""wait_frame.py both.tsv FRAME SECONDS: returns once the recording reaches a frame, stops growing for 20 s, or time's up."""
import os
import sys
import time

path, frame, seconds = sys.argv[1], int(sys.argv[2]), float(sys.argv[3])
begin = time.time()
size, since = -1, time.time()
while time.time() - begin < seconds:
    time.sleep(0.5)
    if not os.path.exists(path):
        continue
    now = os.path.getsize(path)
    if now != size:
        size, since = now, time.time()
    elif time.time() - since > 20:
        print('recording stopped growing: a freeze, or the game closed')
        break
    with open(path, 'rb') as f:
        f.seek(max(0, now - 600))
        lines = f.read().split(b'\n')
    for line in reversed(lines[:-1]):
        head = line.split(b'\t')[0]
        if head.isdigit():
            if int(head) >= frame:
                sys.exit(0)
            break
