@echo off
rem The app-layer ratchet -- see src\_check_app_layer.bas for what it enforces
rem and why it is a program rather than a findstr loop.
cd src
..\toolchains\FreeBASIC-1.10.1-winlibs-gcc-9.3.0\fbc64.exe -gen gas64 -x ..\_check_app_layer.exe _check_app_layer.bas
if errorlevel 1 exit /b 1
cd ..
rem ".\" is required: cmd does not search the current directory for an exe.
.\_check_app_layer.exe
