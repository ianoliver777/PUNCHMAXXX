@echo off
REM PUNCHMAXXX installer - TRIPLE SEVEN AUDIO
REM Re-launch as administrator (Program Files needs it)
net session >nul 2>&1
if %errorlevel% neq 0 (
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

cd /d "%~dp0"
echo.
echo   PUNCHMAXXX by TRIPLE SEVEN AUDIO - installing...
echo.

set "VST3DIR=%CommonProgramFiles%\VST3"
if not exist "%VST3DIR%" mkdir "%VST3DIR%"

if not exist "PUNCHMAXXX.vst3" (
    echo   X  PUNCHMAXXX.vst3 not found. Extract the whole zip first, then run this again.
    goto end
)

if exist "%VST3DIR%\PUNCHMAXXX.vst3" rmdir /s /q "%VST3DIR%\PUNCHMAXXX.vst3"
xcopy "PUNCHMAXXX.vst3" "%VST3DIR%\PUNCHMAXXX.vst3\" /e /i /q /y >nul
if %errorlevel% neq 0 (
    echo   X  Copy failed. Is your DAW open? Close it and try again.
    goto end
)
echo   OK PUNCHMAXXX.vst3  -^>  %VST3DIR%

if exist "PUNCHMAXXX.exe" (
    if not exist "%ProgramFiles%\TRIPLE SEVEN AUDIO" mkdir "%ProgramFiles%\TRIPLE SEVEN AUDIO"
    copy /y "PUNCHMAXXX.exe" "%ProgramFiles%\TRIPLE SEVEN AUDIO\PUNCHMAXXX.exe" >nul
    powershell -NoProfile -Command "$s=(New-Object -ComObject WScript.Shell).CreateShortcut([Environment]::GetFolderPath('CommonStartMenu')+'\Programs\PUNCHMAXXX.lnk'); $s.TargetPath='%ProgramFiles%\TRIPLE SEVEN AUDIO\PUNCHMAXXX.exe'; $s.Save()" >nul 2>&1
    echo   OK PUNCHMAXXX.exe   -^>  Start menu
)

echo.
echo   DONE. Restart your DAW, rescan, and go make some drums slap.

:end
echo.
pause
