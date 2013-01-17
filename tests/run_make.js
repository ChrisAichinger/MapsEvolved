// run_make.js - Execute VS2010's nmake in a target directory
// Usage: cscript.exe run_make.js <DIRECTORY> <MAKEFILE> <ARGS>
// 
// DIRECTORY  - The target directory; cd'ed to before executing nmake.
// MAKEFILE   - The makefile to execute within DIRECTORY. Typically makefile.vc.
// ARGS       - Further arguments to nmake.exe after /F MAKEFILE
//
// If no error occurs, run_make.js returns directly. If an error occurs,
// the user must manually close the nmake window (so he can inspect the errors)



/* DEBUG: Show number of arguments and arguments themselves, \n separated
var text = "";
for (var i = 0; i <= WScript.Arguments.length - 1; i++) {
    text += WScript.Arguments.Item(i) + "\n";
}
WScript.Echo(WScript.Arguments.length + " arguments\n" + text);
//  */

var file_and_args = "";
for (var i = 1; i <= WScript.Arguments.length - 1; i++) {
    file_and_args += '"' + WScript.Arguments.Item(i) + '" ';
}

var cmd = 'cmd.exe /C ' +
          'call "%VS100COMNTOOLS%\\vsvars32.bat" && ' +
          'cd "' + WScript.Arguments.Item(0) +'" && ' + 
          'nmake /f ' + file_and_args + " || pause";

WScript.Echo(cmd);

var shell = WScript.CreateObject("WScript.Shell");
var exitcode = shell.Run(cmd, 1, true);
WScript.Quit(exitcode);
