@echo off
set PATH=C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;%PATH%
cd /d "%~dp0"
if not exist build_mingw\NUL mkdir build_mingw
cmake -S . -B build_mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.0/mingw_64
if errorlevel 1 exit /b 1
cmake --build build_mingw -j %NUMBER_OF_PROCESSORS%
if errorlevel 1 exit /b 1
"C:\Qt\6.11.0\mingw_64\bin\windeployqt.exe" --release --compiler-runtime --no-translations "%~dp0build_mingw\CreationTheoryMorph.exe"
if errorlevel 1 exit /b 1
start "" "%~dp0build_mingw\CreationTheoryMorph.exe"
