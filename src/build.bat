@ECHO OFF
IF NOT DEFINED LIB (call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64)

mkdir B:\build

pushd B:\build
	cl -FC -Zi B:\src\win32_handmade.cpp user32.lib gdi32.lib
popd