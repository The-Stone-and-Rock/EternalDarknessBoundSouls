@echo off
rem Play with separate rooms ON and record everything for Claude: play-rooms-log.bat [NAME]
rem The same game as play-rooms.bat, but through tools/ed/run.sh: every log, both players recorded each frame
rem (both.tsv) and pictures 4 times a second, in logs\run-DATE-TIME-NAME\. Start from the launcher as usual.
rem Close the game window when done; the check at the end prints, then press a key.
cd /d "%~dp0"
set "name=%~1"
if "%name%"=="" set "name=rooms-play"
set ED_ROOM_SLOTS=2
set ED_COOP_STAY=1
set ED_COOP_ENEMY_LOG=1
set ED_SHOW_TRACE=1
set ED_FIN_LOG=1
"C:\Program Files\Git\bin\sh.exe" tools/ed/run.sh launcher %name%
pause
