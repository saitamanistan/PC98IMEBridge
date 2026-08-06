@echo off
setlocal

rem Start the Windows bridge and the PC-98 emulator for the COM1 test.
for %%I in ("%~dp0..") do set "REPO=%%~fI"
if not defined PC98IMEBRIDGE_NP2_DIR set "PC98IMEBRIDGE_NP2_DIR=D:\np21w\np21w-starterset"
if not defined PC98IMEBRIDGE_DOTNET_EXE set "PC98IMEBRIDGE_DOTNET_EXE=C:\Program Files\dotnet\dotnet.exe"
set "NP2=%PC98IMEBRIDGE_NP2_DIR%"
set "DOTNET=%PC98IMEBRIDGE_DOTNET_EXE%"
set "BRIDGE=%REPO%\host\ImeDosBridge\bin\Debug\net8.0-windows\ImeDosBridge.dll"

if not exist "%BRIDGE%" (
    echo Bridge binary not found:
    echo   %BRIDGE%
    echo Build it first with: dotnet build host\ImeDosBridge.sln
    exit /b 1
)
if not exist "%DOTNET%" (
    echo Windows dotnet executable was not found:
    echo   %DOTNET%
    echo Set PC98IMEBRIDGE_DOTNET_EXE to its full path.
    exit /b 1
)
if not exist "%NP2%\np21x64w.exe" (
    echo np21w was not found:
    echo   %NP2%\np21x64w.exe
    echo Set PC98IMEBRIDGE_NP2_DIR to the np21w starter-set directory.
    exit /b 1
)
if not exist "%NP2%\fdosboot.hdi" (
    echo Boot disk was not found:
    echo   %NP2%\fdosboot.hdi
    exit /b 1
)
if not exist "%REPO%\build\pc98\IME98DBG.COM" (
    echo PC-98 client binary not found:
    echo   %REPO%\build\pc98\IME98DBG.COM
    echo Build it first in WSL with: make pc98-debug
    exit /b 1
)
if not exist "%REPO%\build\pc98\IME98TSD.COM" (
    echo PC-98 TSR binary not found:
    echo   %REPO%\build\pc98\IME98TSD.COM
    echo Build it first in WSL with: make pc98-tsr-debug
    exit /b 1
)

copy /y "%REPO%\build\pc98\IME98DBG.COM" "%NP2%\share\IME98.COM" >nul
copy /y "%REPO%\build\pc98\IME98TSD.COM" "%NP2%\share\IME98TSR.COM" >nul
copy /y "%REPO%\samples\IME98.CFG" "%NP2%\share\IME98.CFG" >nul
copy /y "%REPO%\samples\AUTOEXEC.PC98.BAT" "%NP2%\share\AUTOEXEC.BAT" >nul

rem Stop only this project's bridge, not unrelated dotnet applications.
powershell.exe -NoProfile -Command "Get-CimInstance Win32_Process -Filter 'Name = ''dotnet.exe''' | Where-Object CommandLine -Like '*ImeDosBridge.dll*' | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }"
taskkill /f /im np21x64w.exe >nul 2>nul

rem Start in interactive mode so committed Windows IME text is sent to DOS.
start "PC98IMEBridge" powershell.exe -NoProfile -Command "$env:DOTNET_ROLL_FORWARD='Major'; Start-Process -FilePath '%DOTNET%' -ArgumentList '%BRIDGE%','--pipe','NP2-NamedPipe','--pipe-client','--debug-pipe','NP2-ImeDebug' -WorkingDirectory '%REPO%'"
ping 127.0.0.1 -n 3 >nul
pushd "%NP2%"
start "np21w PC-98" "%NP2%\np21x64w.exe"
popd

echo Started ImeDosBridge and np21w.
echo AUTOEXEC installs IME98TSR.COM; foreground verification remains manual.
endlocal
