#!/bin/sh
# Chapter Pages in co-op (b049fc3; chapter-page-readers.md, chapter-page-keep-reader.md): every run a COLD Jump to Game
# (tools/ed/jtg.sh, no save state anywhere), the window off the desktop and muted (ED_RUN_AWAY=1), every log.
# Chapter 12 (Alex End) holds the pages' scripts 1572-1580; the chapter 2 page (item 200, script 1572) is spawned at the
# picker's feet (ED_ITEM_SPAWN=2300,200,200,120,WHO) in room 100, picked up (B x4: EXAMINE, DETAIL/the words, PICK UP,
# the find's words), then used from the Tome (Start, A x3: the item, Use, Yes).
# Run from Git Bash: sh docs/research/build-run-pages.sh [RUN...]
#   RUN: p1 p2 p1a p2a both p1copy solo door; none given: all, in that order.
# JTG_SLOT=2 by default (EDrun2.exe), so these can run beside the Tome trip script (which uses slot 1 and run.sh's
# EternalDarkness.exe); PAGE_SLOT=N picks another.
# Pass, every page run: "[tome-copy] ... asked for a film" (player 2's or player 1's copy) or player 1's real Tome close,
# then the film (mode 6, [room] change 0xA6 0x80 within a few frames of the Tome's close), the chapter's opening
# (cutscene), then "[split] on" in room 166 with both players listed; no saves/reports/hang-*.txt; no ASSERT.
# Solo: the same room changes as logs/run-20261115-113012-jtg-page-solo-film and no [tome-copy]/[stay]/[split] line.
cd "/e/Eternal Darkness Recomp Fork" || exit 1
export ED_RUN_AWAY=1 ED_COOP_LOG=1 ED_RUN_SHOTS_MS=250
SLOT=${PAGE_SLOT:-2}
FR=12000   # frames after the base (2255): the pickup, the Tome, the film and chapter 2's opening to room 166's control
BASE=2255

# Relative press lists (frames from the base).
P1_PICK="100,106,0x0200;160,166,0x0200;220,226,0x0200;280,286,0x0200"
P2_PICK="100,106,b;160,166,b;220,226,b;280,286,b"
P1_USE() { s=$1; printf '%s' "$s,$((s+6)),0x1000;$((s+100)),$((s+106)),0x0100;$((s+160)),$((s+166)),0x0100;$((s+220)),$((s+226)),0x0100"; }
P2_USE() { s=$1; printf '%s' "$s,$((s+6)),start;$((s+100)),$((s+106)),a;$((s+160)),$((s+166)),a;$((s+220)),$((s+226)),a"; }
STAY="ED_ROOM_SLOTS=2 ED_COOP_STAY=1 ED_ROOM_LOG=1"

page() {  # name, who picks up (1/2), then env assignments
    name=$1; who=$2; shift 2
    env JTG_SLOT=$SLOT JTG_BASE=$BASE JTG_FRAMES=$FR ED_RUN_SECONDS=900 ED_ITEM_SPAWN="2300,200,200,120,$who" "$@" \
        sh tools/ed/jtg.sh 12 "page-$name" </dev/null
    R=$(ls -d logs/run-*-jtg-page-"$name" 2>/dev/null | tail -1)
    echo "[pages] $name: $R"
    grep -h "asked for a film\|\[tome-copy\] player [12]'s item\|change 0x000000A6\|\[split\] on\|ASSERT\|WARNING" "$R"/page-*.log 2>/dev/null | head -20
    ls saves/reports/hang-*.txt 2>/dev/null | tail -1
}

run_one() {
case "$1" in
p1)   # Player 1's page from their real Tome, together.
      page p1 1 JTG_P1_SCRIPT="$P1_PICK;$(P1_USE 420)" ;;
p2)   # Player 2's page from their copy Tome, together.
      page p2 2 JTG_P2_SCRIPT="$P2_PICK;$(P2_USE 420)" ;;
p1a)  # Player 1's page with player 2 apart (put into 100's way out to room 51 at +30, before the pickup).
      page p1a 1 $STAY JTG_PUT="30,2,0,850,400" JTG_P1_SCRIPT="$P1_PICK;$(P1_USE 420)" ;;
p2a)  # Player 2's page with player 1 apart (player 1 to room 51 at +300, after player 2's pickup).
      page p2a 2 $STAY JTG_PUT="300,1,0,850,400" JTG_P2_SCRIPT="$P2_PICK;$(P2_USE 520)" ;;
both) # Player 2's page while player 1's real Tome is up (Start at +380, closed by Start at +800).
      page both 2 JTG_P1_SCRIPT="380,386,0x1000;800,806,0x1000" JTG_P2_SCRIPT="$P2_PICK;$(P2_USE 420)" ;;
bothx) # As both, with each slow frame's guest place dumped ([stall] / [pcdump]: ED_STALL_MS, testing): the both-Tomes
      # stall (logs/run-20261115-152853-jtg-page-both, 4 world frames of 2.4 s after the copy's close).
      page bothx 2 ED_STALL_MS=1200 JTG_P1_SCRIPT="380,386,0x1000;800,806,0x1000" JTG_P2_SCRIPT="$P2_PICK;$(P2_USE 420)" ;;
noitem) # Isolating: both Tomes up, player 2 selects the page (A) and backs out (B x3) with no use; player 1's Tome closed
      # by Start at +1000. Does the copy's close beside the real Tome stall on its own (the live sound stop)?
      page noitem 2 ED_STALL_MS=1200 JTG_P1_SCRIPT="380,386,0x1000;1000,1006,0x1000" \
          JTG_P2_SCRIPT="$P2_PICK;420,426,start;520,526,a;600,606,b;660,666,b;720,726,b" ;;
p1copy) # Player 1's page from a COPY Tome: player 1 picks it up, player 2 is put into the way out to room 51 at +345,
      # and player 1's Start at +370 falls inside player 2's door countdown (~62 frames: logs/run-20261115-115907-jtg-page-
      # apart-p1 2286-2348), which opens player 1's Tome as a copy ("[stayer] player 1's Start: their Tome in a copy").
      # The item waits for the door (g_tome_copy_p1_left.pending), then tome_copy_p1_left's film branch: "[tome-copy]
      # player 1's item 1572 asked for film 6".
      page p1copy 1 $STAY JTG_PUT="345,2,0,850,400" JTG_P1_SCRIPT="$P1_PICK;$(P1_USE 370)" ;;
solo) # Solo (co-op off): player 1's page.
      page solo 1 JTG_SET="mod_coop=0" JTG_FRAMES=7500 JTG_P1_SCRIPT="$P1_PICK;$(P1_USE 420)" ;;
door) # Chapter 1: player 2's Tome opened and closed (no item), then player 1 through 341's door (101): the ARAM words.
      env JTG_SLOT=$SLOT JTG_FRAMES=1600 ED_RUN_SCRIPT="2313:101,2373:101" JTG_P2_SCRIPT="60,66,start;200,206,b;260,266,b" \
          sh tools/ed/jtg.sh 1 tome-close-door </dev/null
      R=$(ls -d logs/run-*-jtg-tome-close-door | tail -1); echo "[pages] door: $R"
      grep -h "ARAM\|room\] change\|ASSERT" "$R"/page-*.log | head ;;
*)    echo "[pages] no run named $1" ;;
esac
}

if [ $# -eq 0 ]; then set -- p1 p2 p1a p2a both p1copy solo door; fi
for r in "$@"; do run_one "$r"; done
echo RUNS DONE
