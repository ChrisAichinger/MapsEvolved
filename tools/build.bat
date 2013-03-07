
cd /D "%~dp0"
cd ..
call "%VS100COMNTOOLS%\vsvars32.bat"
msbuild %*
