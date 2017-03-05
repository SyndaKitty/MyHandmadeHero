@ECHO OFF
subst B: D:\Projects\MyHandmadeHero
B:
set Path=%Path%;"C:\Program Files\4Coder"
set Path=%Path%;"B:\misc"
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64