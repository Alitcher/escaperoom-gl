@echo off
rem Double-click to play the sample room.
rem Build first if you changed code: cmake --build build
start "" "%~dp0build\Debug\engine.exe"
