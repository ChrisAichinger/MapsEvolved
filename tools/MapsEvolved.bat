@ECHO OFF
pushd "%~dp0\.."

taskkill /FI "WINDOWTITLE eq Maps Evolved" /FI "IMAGENAME eq python.exe"

call venv\scripts\activate.bat

@ECHO ON
python MapsEvolved.py
@ECHO OFF

choice /T 6 /C kq /N /D q /M "Press 'K' to keep the console open:"
IF %ERRORLEVEL%==1 pause

popd
