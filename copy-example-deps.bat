@echo off
:: copy-example-deps.bat: Copy dependencies for the examples into the
:: right places.  Run from ...\rocketc.  Assumes BASS is in ..\bass24 and
:: SDL 1.2 is in ..\SDL-1.2.15.
:: This is for 32-bit (x86).

mkdir examples\include
mkdir examples\lib

:: BASS

xcopy /f /y /i ..\bass24\c\bass.h examples\include
xcopy /f /y /i ..\bass24\c\bass.lib examples\lib
xcopy /f /y /i ..\bass24\bass.dll examples

:: SDL

xcopy /f /y /i ..\SDL-1.2.15\include\*.* examples\include
xcopy /f /y /i ..\SDL-1.2.15\lib\x86\*.lib examples\lib
xcopy /f /y /i ..\SDL-1.2.15\lib\x86\*.dll examples

:: vi: set ts=2 sts=2 sw=2 et ai: ::

