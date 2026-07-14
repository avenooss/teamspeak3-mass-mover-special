@echo off
setlocal

echo Building TeamSpeak 3 MassMover Plugin for Windows...

if not exist "ts3client-pluginsdk-26\include" (
    echo ERROR: TeamSpeak Plugin SDK v26 was not found in ts3client-pluginsdk-26.
    exit /b 1
)

if not exist "build\windows" mkdir "build\windows"
if not exist "bin\windows" mkdir "bin\windows"

where gcc >nul 2>nul
if not errorlevel 1 goto :mingw_build

where cl >nul 2>nul
if not errorlevel 1 goto :msvc_build

echo ERROR: No supported compiler found. Install MinGW-w64 or use a Visual Studio Developer Command Prompt.
exit /b 1

:mingw_build
echo Using MinGW GCC compiler
gcc -c -O2 -Wall -DWIN32 -Its3client-pluginsdk-26/include -Icjson src/massmover.c -o build/windows/massmover.o || exit /b 1
gcc -c -O2 -Wall -Icjson cjson/cJSON.c -o build/windows/cJSON.o || exit /b 1
gcc -shared -o bin/windows/massmover.dll build/windows/massmover.o build/windows/cJSON.o || exit /b 1
goto :success

:msvc_build
echo Using Microsoft Visual C++ compiler
cl /nologo /c /O2 /DWIN32 /I"ts3client-pluginsdk-26/include" /I"cjson" src/massmover.c /Fo"build/windows/massmover.obj" || exit /b 1
cl /nologo /c /O2 /I"cjson" cjson/cJSON.c /Fo"build/windows/cJSON.obj" || exit /b 1
link /NOLOGO /DLL /OUT:"bin/windows/massmover.dll" build/windows/massmover.obj build/windows/cJSON.obj || exit /b 1

:success
echo SUCCESS: bin\windows\massmover.dll
echo See README.md for installation instructions.
exit /b 0
