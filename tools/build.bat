@ECHO OFF

cd %~dp0\..\ODM && ^
msbuild /p:Configuration=Debug && ^
cd %~dp0\.. && ^
python configure.py && ^
ODM\libraries\unxutils\make.exe
