@echo off

setlocal

where cl >nul 2>nul
IF %ERRORLEVEL% NEQ 0 (echo WARNING: cl is not in the path - please set up Visual Studio to do cl builds. To do that, run "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" or equivalent for your Visual Studio version) && goto SkipMSVC

echo -------------------------------------
echo Building with MSVC

if not exist "build" mkdir build
pushd build
call cl -I../src -nologo -FC -Oi -O2 -EHsc -std:c++14 -arch:AVX2 %* ..\src\main.cpp ..\src\portable-memory-mapping\MemoryMapped.cpp -Fevsign.exe
popd

:SkipMSVC
