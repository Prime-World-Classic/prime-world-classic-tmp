@echo off
rem Prime World: put a freshly built PW_Game.exe next to the runtime DLLs and start it.
rem   deploy_dev_build.bat                    - x64 ReleaseSingleExe (cheats/console/asserts):
rem                                             build\PW_Game_Release.exe from build_client_x64_win.cmd --release
rem   deploy_dev_build.bat x64ship            - x64 ShippingSingleExe: build\PW_Game.exe
rem   deploy_dev_build.bat x64 nostart        - x64 Release, copy only
rem   deploy_dev_build.bat ReleaseSingleExe   - legacy x32 (VS2008, Src\_<cfg>\PW_Game.exe) into Bin_Dev
rem   deploy_dev_build.bat ShippingSingleExe  - legacy x32 public build into Bin_Dev
rem Result folder: pw\branches\r1117\Bin_Dev64 for x64, Bin_Dev for x32 (Bin_Original is left untouched)
setlocal
set "REPO=%~dp0"
set "BR=%REPO%pw\branches\r1117"
set "CFG=%~1"
if "%CFG%"=="" set "CFG=x64"

if /I "%CFG%"=="x64"     goto x64
if /I "%CFG%"=="x64ship" goto x64

rem ---------------------------------------------------------------- legacy x32
set "EXE=%BR%\Src\_%CFG%\PW_Game.exe"
set "DST=%BR%\Bin_Dev"

if not exist "%EXE%" (
  echo Built exe not found: %EXE%
  echo Build PW_Game in Visual Studio first, configuration %CFG% ^| Win32.
  pause
  exit /b 1
)

if not exist "%DST%" mkdir "%DST%"

echo Copying runtime files from Bin_Original...
robocopy "%BR%\Bin_Original" "%DST%" /E /XO /XF PW_Game.exe /NJH /NJS /NDL /NFL /NP >nul
if exist "%BR%\Bin\config.txt" copy /Y "%BR%\Bin\config.txt" "%DST%\" >nul
goto copyexe

rem ---------------------------------------------------------------- x64
:x64
set "EXE=%REPO%build\PW_Game_Release.exe"
if /I "%CFG%"=="x64ship" set "EXE=%REPO%build\PW_Game.exe"
set "DST=%BR%\Bin_Dev64"

if not exist "%EXE%" (
  echo Built exe not found: %EXE%
  if /I "%CFG%"=="x64ship" (echo Run build_client_x64_win.cmd first.) else (echo Run build_client_x64_win.cmd --release first.)
  pause
  exit /b 1
)

if not exist "%DST%" mkdir "%DST%"

rem Bin_Original holds x32 DLLs: take only the non-binary files from it, x64 DLLs come from Vendor.
echo Copying runtime files from Bin_Original (no binaries)...
robocopy "%BR%\Bin_Original" "%DST%" /E /XO /XF *.dll *.exe *.pdb *.manifest /NJH /NJS /NDL /NFL /NP >nul
if exist "%BR%\Bin\config.txt" copy /Y "%BR%\Bin\config.txt" "%DST%\" >nul

echo Copying x64 DLLs...
set "V=%BR%\Vendor"
for %%f in (
  "%V%\ACE_wrappers\lib\x64\ACE.dll"
  "%V%\Terabit\lib\x64\IOTerabit.dll"
  "%V%\Terabit\lib\x64\TProactor.dll"
  "%BR%\Tools\Censor\lib\Release\x64\CensorDll.dll"
  "%V%\zlib\lib\x64\zlib1.dll"
  "%V%\fmod\fmodex64.dll"
  "%V%\fmod\fmod_event64.dll"
  "%V%\fmod\fmod_event_net64.dll"
  "%V%\Steam\1.23a\redistributable_bin\win64\steam_api64.dll"
) do (
  copy /Y %%f "%DST%\" >nul || (echo Copy failed: %%f & pause & exit /b 1)
)
rem d3dx9_43 x64 is not in the repository: it comes with the DirectX End-User Runtime (June 2010)
if exist "%SystemRoot%\System32\d3dx9_43.dll" (
  copy /Y "%SystemRoot%\System32\d3dx9_43.dll" "%DST%\" >nul
) else (
  echo WARNING: %SystemRoot%\System32\d3dx9_43.dll not found - install the DirectX End-User Runtime.
)

:copyexe
echo Copying %EXE%...
copy /Y "%EXE%" "%DST%\PW_Game.exe" >nul || (echo Copy failed - is the game still running? & pause & exit /b 1)
if exist "%EXE%.manifest" copy /Y "%EXE%.manifest" "%DST%\" >nul

rem Claude can't overwrite files the running game keeps open (main.swf) and leaves *_new.* next to them
set "FLASH=%BR%\Data\UI\Screens\Combat\Flash"
if exist "%FLASH%\main_new.swf" (
  echo Updating combat UI main.swf...
  move /Y "%FLASH%\main_new.swf" "%FLASH%\main.swf" >nul || (echo main.swf is locked - is the game still running? & pause & exit /b 1)
)

if /I "%~2"=="nostart" exit /b 0
echo Starting the game...
start "" /D "%DST%" "%DST%\PW_Game.exe"
