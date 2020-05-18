@echo off

rem
rem Quick and dirty "tool" to package a new NGPCarMenu_<versionTag>.zip release file.
rem Remember to do "clean - rebuild all" with Release build in VC++ before packaging the release ZIP file.
rem


SET APPNAME=NGPCarMenu
SET VERSIONTAG=%~1
SET RELEASE_FOLDER=Release\VER_%VERSIONTAG%
SET RELEASE_PKG=%APPNAME%_%VERSIONTAG%.zip

SET ZIP_TOOL=C:\Program Files\7-Zip\7z.exe

echo [%DATE% %TIME%] %~nx0 %APPNAME% %VERSIONTAG%
echo [%DATE% %TIME%] Release folder  %RELEASE_FOLDER%
echo [%DATE% %TIME%] Release package %RELEASE_PKG%


if "%~1" == "" (
 echo Missing cmdline argument: versionTag
 echo Example: CreateReleaseZip.cmd 1.14
 goto END
)

if NOT EXIST "Release" (
 echo Release folder missing. Re-build the project in C++ compiler usign Release configuration
 goto END
)

if NOT EXIST "Release\%APPNAME%.dll" (
 echo %APPNAME%.dll Re-build the project in C++ compiler using Release configuration
 goto END
)

echo Do you want to create a release package %APPNAME%_%VERSIONTAG%.zip?
echo Press CTRL-C to quit, other key to continue...
pause


mkdir "%RELEASE_FOLDER%\"
mkdir "%RELEASE_FOLDER%\Replays\"
mkdir "%RELEASE_FOLDER%\Plugins\"
mkdir "%RELEASE_FOLDER%\Plugins\%APPNAME%\"
mkdir "%RELEASE_FOLDER%\Plugins\%APPNAME%\preview\1920x1080\"
mkdir "%RELEASE_FOLDER%\Plugins\%APPNAME%\preview\1366x768\"

rem Dummy files because 7Zip tool would ignore empty folders
type NUL > "%RELEASE_FOLDER%\Plugins\%APPNAME%\preview\1920x1080\carImages.txt"
type NUL > "%RELEASE_FOLDER%\Plugins\%APPNAME%\preview\1366x768\carImages.txt"

copy "Release\%APPNAME%.dll" "%RELEASE_FOLDER%\Plugins\"
copy "%APPNAME%.ini"         "%RELEASE_FOLDER%\Plugins\"
copy "%APPNAME%.rpl"         "%RELEASE_FOLDER%\Replays\"
copy "CustomCarSpecs.ini"    "%RELEASE_FOLDER%\Plugins\%APPNAME%\"
copy "..\LicenseText.txt"    "%RELEASE_FOLDER%\Plugins\%APPNAME%\"
copy "..\LicenseText_3rdPartyTools.txt" "%RELEASE_FOLDER%\Plugins\%APPNAME%\"
copy "..\ReadMe.md"          "%RELEASE_FOLDER%\Plugins\%APPNAME%\"
copy "..\ReadMe.md"          "%RELEASE_FOLDER%\Plugins\%APPNAME%\ReadMe.txt"

PUSHD "%RELEASE_FOLDER%\"
del "..\%RELEASE_PKG%"
"%ZIP_TOOL%" a -r -tzip "..\%RELEASE_PKG%" *.*
POPD

echo [%DATE% %TIME%] Release\VER_%VERSIONTAG% package build completed

:END
