@ECHO OFF
set VENV_DIR=%1

IF NOT [%VENV_DIR%] == [] GOTO SKIP_DEFAULT_VENV
set VENV_DIR=%~dp0\..\venv
:SKIP_DEFAULT_VENV

CALL "%VS100COMNTOOLS%\vsvars32.bat"
CALL "%VENV_DIR%\Scripts\activate.bat"
@ECHO OFF

shift
IF ["%1"] == [""] GOTO DEFAULT_EXEC
    "%1" "%2" "%3" "%4" "%5" "%6" "%7" "%8" "%9"
    exit
    GOTO ENDSCRIPT

:DEFAULT_EXEC
cmd.exe

:ENDSCRIPT
