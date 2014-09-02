@ECHO OFF

cd %~dp0\..\ODM && ^
msbuild /p:Configuration=Debug && ^
cd %~dp0\.. && ^
python configure.py && ^
__unxutils\make.exe
