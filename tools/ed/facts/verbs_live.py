#!/usr/bin/env python3
"""What verbs.md marks "proved" from live memory: every value read from every save state, and the code's own literals beside them.
Run from the repo root: python -I tools/ed/facts/verbs_live.py
Prints a line per save, then PASS or FAIL per check. Nothing is written and saves/ is only read."""
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import h
from h import IDX, TXT, callers
import savestate

ROOT = h.ROOT
D = h._dol()
SDA = 0x80625420
SLOTS = 0x80335A38
LOOKS = 0x80307108
WORDS = 0x805CAA40
fails = 0


def check(ok, what):
    global fails
    print(('PASS  ' if ok else 'FAIL  ') + what)
    if not ok:
        fails += 1


def sgn16(v):
    return v - 65536 if v >= 32768 else v


def nodes(S):
    n = S.u32(S.u32(S.r13 - 21468) + 4)
    while n:
        yield n
        n = S.u32(n + 4)


saves = {os.path.basename(p)[:-6]: savestate.State(p) for p in sorted(glob.glob(os.path.join(ROOT, 'saves', 'slot*.state')))}

print('== each save: room, prompt slots {priority, button, id, script, skip, slot, flag}, the two looks, the script words')
for name, S in saves.items():
    room = S.s32(SDA - 23276)
    print(name, 'room', room)
    for i in range(7):
        a = SLOTS + 28 * i
        r = [S.s32(a), S.s32(a + 4), S.s32(a + 8), S.s32(a + 12), S.s32(a + 16), S.s32(a + 20), S.u16(a + 24)]
        if r[2] != -1 and r[1] != -1:
            print('   slot', i if i < 6 else 'shown', r)
    for k in (0, 1):
        a = LOOKS + 32 * k
        print('   look', k, [S.s32(a + 4 * j) for j in range(8)])
    w = [(sgn16(S.u16(WORDS + 8 * i)), sgn16(S.u16(WORDS + 8 * i + 2)), S.u16(WORDS + 8 * i + 4), S.u16(WORDS + 8 * i + 6)) for i in range(10)]
    print('   words', [x for x in w if x[0]])

print()
print('== checks against memory')
# Bindings: the table in memory is the table in main.dol
same = True
for addr, count in ((0x8023B9B8, 11), (0x8023AF40, 9), (0x8023AE60, 7), (0x802549E4, 3), (0x8023F1A8, 2), (0x802448E0, 2), (0x80244880, 3)):
    for i in range(count):
        for k in range(5):   # mask, when, handler, r4, the two shorts. +20 on is the row's live state
            for S in saves.values():
                got, want = S.u32(addr + 32 * i + 4 * k), D.word(addr + 32 * i + 4 * k)
                if addr == 0x802549E4 and k == 3:   # the triggers' r4 is filled at run time with the control block
                    want = S.u32(S.r13 - 25660)
                if got != want:
                    same = False
check(same, 'first 20 bytes of each row in binding tables 0x8023B9B8, 0x8023AF40, 0x8023AE60, 0x802549E4, 0x8023F1A8, 0x802448E0, 0x80244880: memory equals main.dol in every save')
ch = {n: (S.u32(S.u32(S.r13 - 26224) + 4), S.u32(S.u32(S.r13 - 26224))) for n, S in saves.items()}
check(all(v == (0x8023B9B8, 0x80006900) for v in ch.values()), 'in-play channel r13-26224: +4 is 0x8023B9B8, +0 is 0x80006900, in every save')
who_ok = True
for n, S in saves.items():
    who = S.u32(S.r13 - 26524)
    twos = {S.s32(x + 16): S.u32(x + 52) for x in nodes(S) if S.s32(x + 8) == 2}
    if twos.get(1) != who or any(m == who for h_, m in twos.items() if h_ != 1):
        who_ok = False
check(who_ok, "r13-26524 is the model (+52) of kind 2 handle 1 in every save, and not the second kind 2's in saves 9 and 10")
# fn25 / the room: r13-23276 against the format string
check(D.cstr(SDA - 32760) == 'RM: %i', "0x80006B80 passes r13-23276 with the string at r13-32760: %r" % D.cstr(SDA - 32760))
vals = {n: S.s32(SDA - 23276) for n, S in saves.items()}
check(vals.get('slot1') == 147 and vals.get('slot3') == 21 and vals.get('slot7') == 155 and vals.get('slot10') == 115 and vals.get('slot6') == 153,
      'r13-23276 live: %s' % vals)
# The zone slots' +0: the code never writes it, so it stays 0 while the slot is in use
zone = []
for n, S in saves.items():
    for i in (2, 4):
        a = SLOTS + 28 * i
        if S.s32(a + 8) != -1 and S.s32(a + 4) != -1:
            zone.append((n, i, S.s32(a), S.s32(a + 8), S.s32(a + 12)))
print('   zone slots in use (save, slot, +0, id, script):', zone)
check(zone and all(z[2] == 0 for z in zone), 'slots 2 and 4: priority word is 0 in all of them, as the store list at 0x801175BC..0x801175CC has no +0')
# The live prompt with the asker's own literal
hits = []
for n, S in saves.items():
    for i in range(6):
        a = SLOTS + 28 * i
        if S.s32(a + 8) == 19 and S.s32(a + 12) == 2246:
            hits.append((n, i, S.s32(a), S.s32(a + 4)))
    a = SLOTS + 28 * 6
    if S.s32(a + 8) == 19 and S.s32(a + 12) == 2246:
        hits.append((n, 'shown', S.s32(a), S.s32(a + 4), S.s32(a + 20)))
print('   prompt 19 with script 2246 in memory:', hits)
check(bool(hits), 'the thing-in-the-world pick up (2, 19, 2246, 0, 49, 1, 0) seen live')
# Looks
S = saves['slot7']
L0 = [S.s32(LOOKS + 4 * j) for j in range(8)]
L1 = [S.s32(LOOKS + 32 + 4 * j) for j in range(8)]
check(L0 == [0, 2139, 5, 139, 1, -742878507, 4, 69], 'slot7 look 0 is %s; L03/2139 is ed221(139, 1, 3552088789, 1, 645, -1); 3552088789 as a signed word is %d' % (L0, 3552088789 - (1 << 32)))
check(L1 == [3, 347, 0, 62, 6, 15429850, 1, 71], 'slot7 look 1 is %s; L03/347 is ed221(62, 6, 15429850, 1, 2156, -1)' % (L1,))
ids = {S.s32(n + 16): (S.u32(S.u32(n + 36) + 24) if S.u32(n + 36) else None) for n in nodes(S)}
check(ids.get(69) == 645 and ids.get(71) == 2156, 'slot7: handle 69 has id %s, handle 71 has id %s (node +36 -> +24)' % (ids.get(69), ids.get(71)))
# A press clears the slot, a zone ask stays: script word next to a waiting prompt
S = saves['slot2']
w = [(sgn16(S.u16(WORDS + 8 * i)), sgn16(S.u16(WORDS + 8 * i + 2))) for i in range(10)]
check((2129, 1) in w, 'slot2: script word table at 0x805CAA40 holds script 2129 with word 1, beside prompt {2, 6, 2129}: %s' % [x for x in w if x[0]])
# Kind 32: one node in every save, the handler that reads the shown prompt
counts = {n: sum(1 for x in nodes(S) if S.s32(x + 8) == 32) for n, S in saves.items()}
check(all(v == 1 for v in counts.values()), 'one node of kind 32 in every save: %s' % counts)
# Slot 8's look 1: a kind 3 thing taken
S = saves['slot8']
check([S.s32(LOOKS + 32 + 4 * j) for j in (0, 1, 3, 4, 6, 7)] == [2, 2369, 62, 1, 0, -1], 'slot8 look 1 is kind 2, script 2369, group 62, entry 1, flags 0, handle -1: the not found form of L03/2369')
# Kind 30, the things
S = saves['slot1']
got = sorted(S.s32(x + 16) for x in nodes(S) if S.s32(x + 8) == 30)
check(got == [62, 65, 66, 67, 68, 81, 82], 'slot1 kind 30 handles: %s' % got)
ids30 = {S.s32(x + 16): S.u32(S.u32(x + 36) + 24) for x in nodes(S) if S.s32(x + 8) == 30}
print('   slot1 kind 30 ids:', ids30)
print('   kind 2 nodes:', {n: [(S.s32(x + 16), S.s32(x + 20)) for x in nodes(S) if S.s32(x + 8) == 2] for n, S in saves.items()})
# Regions against the file are checked by verbs_regions.py. Here the fields the claim names:
S = saves['slot1']
r = S.u32(0x8058AFC0 + (S.u32(0x8058AFC0) << 2) + 12)
lst = S.u32(r + 180)
reg = {}
for i in range(S.u16(r + 176)):
    a = lst + 116 * i
    reg[S.cstr(a + 4)] = a


def r16(a, o):
    return sgn16(S.u16(a + o))


check(r16(reg['ExitRgn01'], 108) == 0 and r16(reg['ExitRgn01'], 58) == 1097 and r16(reg['ExitRgn01'], 60) == -1, 'slot1 ExitRgn01: sphere, enter 1097, leave -1')
check(r16(reg['MiddleDeskRgn'], 108) == 1 and r16(reg['MiddleDeskRgn'], 60) == 2129 and r16(reg['MiddleDeskRgn'], 62) == 1300, 'slot1 MiddleDeskRgn: box, enter 2129, leave 1300')
check(r16(reg['HorrorPlane01'], 108) == 2 and r16(reg['HorrorPlane01'], 100) == -1 and r16(reg['HorrorPlane01'], 102) == 379, 'slot1 HorrorPlane01: plane, +100 -1, +102 379')
check(r16(reg['Region05'], 58) == 2449 and r16(reg['Region05'], 60) == 1300, 'slot1 Region05: sphere, enter 2449, leave 1300')
check(S.u32(reg['CameraRgn01'] + 104) & 1 == 1, 'slot1 CameraRgn01 +104 bit 0 set, the player inside')


# ---- the code's literals
def li_before(site, reg_name, value, back=14):
    i = IDX[site]
    for k in range(i - 1, max(i - back, 0), -1):
        m = re.match(r'li\s+%s, (-?\d+)$' % reg_name, TXT[k])
        if m:
            return int(m.group(1)) == value
    return False


print()
print('== checks against the code')
STATE = {74: [0x800C04C8, 0x800906C8], 71: [0x80059778], 95: [0x800C7994], 48: [0x8005D120, 0x800C89F0], 75: [0x800799D0], 72: [0x80079B18], 76: [0x80079C28],
         77: [0x80079854], 90: [0x80079E20], 30: [0x800C35BC], 52: [0x80074E64], 7: [0x800638B4], 85: [0x800C2C70], 6: [0x8005A524], 108: [0x800C1064], 96: [0x800BDCA4],
         40: [0x8005A47C], 87: [0x8005A374], 84: [0x800C3A88], 39: [0x8005522C], 88: [0x80055CCC], 122: [0x80057BBC], 64: [0x800546FC], 67: [0x80059E20], 69: [0x80059F2C],
         56: [0x80090E18, 0x8008B390]}
bad = [(st, hex(a)) for st, sites in STATE.items() for a in sites if not (TXT[IDX[a]].startswith('bl      0x801F64A0') and li_before(a, 'r4', st))]
check(not bad, 'every state setter listed: li r4, state, then bl 0x801F64A0. Wrong: %s' % bad)
DISPATCH = {1: 0x80058BE8, 74: 0x80052384, 71: 0x800527A8, 95: 0x80052A98, 48: 0x80052CBC, 75: 0x800530F4, 72: 0x800533A8, 76: 0x80053658, 77: 0x800538D8, 90: 0x80053BBC, 30: 0x80051F0C,
            56: 0x80053E4C, 67: 0x8005713C, 69: 0x80057790, 52: 0x800566AC, 53: 0x80056CF0, 7: 0x80054188, 85: 0x800564C0, 6: 0x8005430C, 108: 0x800547D4, 96: 0x80054AF4, 40: 0x80055188,
            87: 0x80055868, 84: 0x80054E70, 39: 0x800553C8, 88: 0x80056028, 122: 0x80057CB0, 64: 0x80057F68}
seen = {}
cur = None
for k in range(IDX[0x8005AA30], IDX[0x8005AA30] + 400):
    m = re.match(r'cmpwi\s+r20, (\d+)$', TXT[k])
    if m:
        cur = int(m.group(1))
    m = re.match(r'bl\s+0x(8[0-9A-F]{7})', TXT[k])
    if m and cur is not None:
        seen.setdefault(cur, int(m.group(1), 16))
bad = [s for s, f in DISPATCH.items() if seen.get(s) != f]
check(not bad, 'state to function at 0x8005AA30: every row of the table. Wrong: %s' % bad)
# 28 asks by their literals
check(len(callers(0x80117404)) == 31, '0x80117404 has 31 bl sites')
check(TXT[IDX[0x80007048]].startswith('bl      0x801642D4') and TXT[IDX[0x8000704C]].startswith('bl      0x8013E154'), '0x8000704C follows bl 0x801642D4')
check(D.word(0x80244CAC + 4 * 6) == 0x8007BD30 and TXT[IDX[0x8007AAE4]].startswith('cmplwi  r31, 0x000B'), 'ed56(6) is the exit 0x8007BD30; n above 11 exits')
check(callers(0x8007F254) == [0x8007B504] and D.word(0x802448A8) == 0x8007F254 and D.word(0x802448C8) == 0x8007F254, '0x8007F254 (asks 55) is called only in ed56(8) and sits in its table')
check(callers(0x80134A44) == [0x801335C4] and callers(0x801170D4) == [0x801349C0], '0x80134A44 is called only at 0x801335C4; 0x801170D4 only at 0x801349C0')
check(len(callers(0x80024528)) == 5 and 0x80012BE8 in callers(0x80024528), 'the find 0x80024528 has 5 sites, ed52 at 0x80012BE8 among them')
check(TXT[IDX[0x80134824]].startswith('cmpwi   r23, 1'), 'a plane is skipped when r23 is 1')
check(TXT[IDX[0x800176C4]].startswith('bc    12, 0, 0x800177B0'), 'ed221 leaves with no ask when +16 is negative')
check(TXT[IDX[0x80163ED0]].startswith('cmplwi  r0, 0x0000') and TXT[IDX[0x80163ECC]].startswith('lhz     r0, 6(r3)'), '0x80163E84 keeps an entry whose +6 is set')
check(sorted(callers(0x8007E508)) == [0x8007A9AC, 0x80083598], '0x8007E508 (asks 53) is called at 0x8007A9AC (ed68(0)) and 0x80083598 only')

print()
print('FAILS', fails)
sys.exit(1 if fails else 0)
