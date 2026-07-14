@echo off
echo Building TeamSpeak 3 MassMover Plugin for Windows...

rem Create directories
if not exist "build\windows" mkdir "build\windows"
if not exist "bin\windows" mkdir "bin\windows"

rem Check for compiler
where gcc >nul 2>nul
if %ERRORLEVEL% == 0 (
    echo Using MinGW GCC compiler
    goto :mingw_build
)

where cl >nul 2>nul
if %ERRORLEVEL% == 0 (
    echo Using Microsoft Visual C++ compiler
    goto :msvc_build
)

echo ERROR: No supported compiler found. Please install MinGW or Visual Studio.
echo MinGW: https://www.mingw-w64.org/
echo Visual Studio: https://visualstudio.microsoft.com/
pause
goto :end

:mingw_build
gcc -c -O2 -Wall -DWIN32 -Its3client-pluginsdk-26/include -Icjson src/massmover.c -o build/windows/massmover.o
if %ERRORLEVEL% neq 0 (
    echo ERROR: Compilation failed
    pause
    goto :end
)

gcc -c -O2 -Wall -Icjson cjson/cJSON.c -o build/windows/cJSON.o
if %ERRORLEVEL% neq 0 (
    echo ERROR: cJSON compilation failed
    pause
    goto :end
)

gcc -shared -o bin/windows/massmover.dll build/windows/massmover.o build/windows/cJSON.o
if %ERRORLEVEL% neq 0 (
    echo ERROR: Linking failed
    pause
    goto :end
)
goto :success

:msvc_build
cl /c /O2 /DWIN32 /I"ts3client-pluginsdk-26/include" /I"cjson" src/massmover.c /Fo"build/windows/massmover.obj"
if %ERRORLEVEL% neq 0 (
    echo ERROR: Compilation failed
    pause
    goto :end
)

cl /c /O2 /I"cjson" cjson/cJSON.c /Fo"build/windows/cJSON.obj"
if %ERRORLEVEL% neq 0 (
    echo ERROR: cJSON compilation failed
    pause
    goto :end
)

link /DLL /OUT:"bin/windows/massmover.dll" build/windows/massmover.obj build/windows/cJSON.obj
if %ERRORLEVEL% neq 0 (
    echo ERROR: Linking failed
    pause
    goto :end
)
goto :success

:success
echo.
echo SUCCESS: Plugin built successfully!
echo Output: bin/windows/massmover.dll
echo.
echo To install:
echo 1. Copy bin/windows/massmover.dll to your TeamSpeak plugins directory:
echo    %APPDATA%\TS3Client\plugins\
echo 2. Restart TeamSpeak
echo 3. Enable the plugin in Settings ^> Plugins
echo.
pause

:end
