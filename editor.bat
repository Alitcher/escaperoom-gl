@echo off
rem Double-click to open the level editor.
rem Build first if you changed code: cmake --build build
start "" "%~dp0build\Debug\engine.exe" --edit
