@ECHO OFF

cd %~dp0\..\ODM && ^
msbuild /p:Configuration=Debug-DLL && ^
cd %~dp0\.. && ^
python configure.py && ^
nmake
