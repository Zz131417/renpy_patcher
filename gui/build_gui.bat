@echo off
REM ============================================================
REM  构建 Ren'Py Patcher GUI（仅 Windows / .NET Framework）
REM  前置条件：
REM    1) Visual Studio 2019 或 2022（含“使用 C++ 的桌面开发” workload）
REM    2) vcpkg：  vcpkg install zlib
REM                 vcpkg integrate install
REM ============================================================
setlocal
set CFG=%1
if "%CFG%"=="" set CFG=Release

echo 正在构建 patcher_gui (%CFG%|x64) ...
msbuild gui\patcher_gui.vcxproj /p:Configuration=%CFG% /p:Platform=x64
if %ERRORLEVEL%==0 (
    echo.
    echo 构建成功！输出：gui\x64\%CFG%\patcher_gui.exe
) else (
    echo 构建失败，请检查上方错误。
)
endlocal
pause
