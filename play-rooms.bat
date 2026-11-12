@echo off
rem Play the Windows build with the players in separate rooms (R1-R6): play-rooms.bat [disc image].
rem The same as play.bat, with the two switches on: ED_ROOM_SLOTS=2 (two rooms loaded at once, 56 MB)
rem and ED_COOP_STAY=1 (the other player stays in their room at a door). ED_COOP_ENEMY_LOG=1 writes
rem the enemies' [enemy] lines to the log. Quick-save states made without the switches won't load here
rem (and ones made here won't load in play.bat). The log is logs\play-rooms.log.
cd /d "%~dp0"
set /p id=<generated\game_id.txt
set "iso=%~1"
if "%iso%"=="" set "iso=dump\%id%.iso"
if not exist logs mkdir logs
if exist logs\play-rooms.log move /y logs\play-rooms.log logs\play-rooms-prev.log >nul
set ED_ROOM_SLOTS=2
set ED_COOP_STAY=1
set ED_COOP_ENEMY_LOG=1
build-win\EternalDarkness.exe generated\main.dol --iso "%iso%" --max-blocks 1000000000000000 > logs\play-rooms.log 2>&1
