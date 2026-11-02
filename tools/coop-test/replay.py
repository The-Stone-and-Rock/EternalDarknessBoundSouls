# Temporary test build: replays button presses from the environment, so a session can be
# played again from its log. Never commit it applied; every line it adds ends in // TWTEST.
#   python -I tools/coop-test/replay.py add        then build, then run with
#   ED_TW_PRESS="31004,1000;31053,100" ED_TW_P2="31195,1000"   (frame,buttons in hex; held 8 frames)
#   python -I tools/coop-test/replay.py remove
# Buttons: Start 1000, A 100, B 200, L 40, R 20, up 8, down 4, left 1, right 2.
# The press lines come from a run's log ("player N's pad: buttons ... frame F"). Presses closer
# than about 30 frames can be lost. A real pad's analog triggers still get through: nobody
# should hold a controller during a replay.
import sys

PRESS = """static void tw_test_press(CPUState* cpu, DolPadState* state, const char* env) {
    const char* t = getenv(env);
    const unsigned long long now = cpu->timebase / 675000ull;
    state->button = 0u;
    state->stick_x = state->stick_y = 0;
    while (t != NULL && *t != 0) {
        unsigned long long at = 0;
        unsigned mask = 0;
        if (sscanf(t, "%llu,%x", &at, &mask) != 2)
            break;
        if (now >= at && now < at + 8u) {
            state->button |= (u16)mask;
            if ((mask & 0x100u) != 0u)
                state->analog_a = 0xFFu;
            if ((mask & 0x20u) != 0u)
                state->trigger_right = 0xFFu;
            if ((mask & 0x40u) != 0u)
                state->trigger_left = 0xFFu;
            state->error = 0;
        }
        t = strchr(t, ';');
        t = t != NULL ? t + 1 : NULL;
    }
}
"""

p = 'runtime/host/hle.c'
s = open(p, newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'


def after(anchor, text):
    global s
    anchor = anchor.replace('\n', nl)
    assert s.count(anchor) == 1, anchor
    s = s.replace(anchor, anchor + text + '  // TWTEST' + nl)


if sys.argv[1] == 'remove':
    s = nl.join(line for line in s.split(nl) if '// TWTEST' not in line)
    assert 'tw_test_press' not in s
else:
    before = 'static void coop_menu_input(CPUState* cpu, bool menu, bool cutscene, DolPadState* pad);' + nl
    assert s.count(before) == 1
    s = s.replace(before, ''.join(line + '  // TWTEST' + nl for line in PRESS.splitlines()) + before)
    after('        controls_read_player2(&state[0]);\n        controls_set_tome_read(-1);\n',
          '        if (getenv("ED_TW_P2") != NULL) tw_test_press(cpu, &state[0], "ED_TW_P2");')
    after('    u32 motor_mask = dol_platform_pad_read(state);\n    controls_set_tome_read(-1);\n',
          '    if (getenv("ED_TW_PRESS") != NULL) tw_test_press(cpu, &state[0], "ED_TW_PRESS");')
    anchor = '    controls_read_player2(&p2);' + nl + '    if (hle_coop_tome_shares() && owner == 1) {' + nl
    assert s.count(anchor) == 1, anchor
    s = s.replace(anchor, anchor.replace(
        '(&p2);' + nl, '(&p2);' + nl + '    if (getenv("ED_TW_P2") != NULL) tw_test_press(cpu, &p2, "ED_TW_P2");  // TWTEST' + nl))
open(p, 'w', newline='').write(s)
