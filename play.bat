@echo off
rem Play the Windows build: play.bat [disc image] (default dump\<game id>.iso).
rem Keyboard and controller as in play.sh. Close the window to quit.
cd /d "%~dp0"
set /p id=<generated\game_id.txt
set "iso=%~1"
if "%iso%"=="" set "iso=dump\%id%.iso"
if not exist logs mkdir logs
rem Co-op separate rooms on (the user, 2026-11-12): two rooms loaded at once (ED_ROOM_SLOTS=2, 56 MB) and the other
rem player stays in their room at a door (ED_COOP_STAY=1). Quick-save states made without them won't load (memory card
rem saves are fine). play-rooms-log.bat plays the same with everything recorded for Claude.
set ED_ROOM_SLOTS=2
set ED_COOP_STAY=1
rem Keep the last session's log: it is the only record of what went wrong.
if exist logs\play.log move /y logs\play.log logs\play-prev.log >nul
build-win\EternalDarkness.exe generated\main.dol --iso "%iso%" --max-blocks 1000000000000000 > logs\play.log 2>&1
