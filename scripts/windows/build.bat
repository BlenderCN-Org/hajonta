@echo off

set BUILDDIR="%~dp0\..\..\build"

IF NOT EXIST %BUILDDIR% mkdir %BUILDDIR%
pushd %BUILDDIR%

cl /nologo /Zi ..\source\hajonta\platform\win32.cpp /link /incremental:no User32.lib

popd
