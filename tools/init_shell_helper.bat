@REM Do not call directly, use init_shell.py!

@IF NOT [%1] == [] CALL deactivate.bat
@CALL "%VS100COMNTOOLS%\vsvars32.bat"
@CALL "%2\Scripts\activate.bat"
@"%3" "%4" "%5" "%6" "%7" "%8" "%9"
