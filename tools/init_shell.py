#!/usr/bin/env python3.3

import sys
import os
import json
import base64
import subprocess

def shell_encode(args):
    """Serialize args to a shell-compatible string

    Turn args into a string that can be passed through the shell (cmd.exe or
    sh) without it being split into multiple args or altered in any other way.
    Use base32 for that and take care to eliminate tailing '=' padding
    characters.
    """

    b32 = base64.b32encode(json.dumps(args).encode('ascii'))
    return b32.decode('ascii').replace('=', 'x')

def shell_decode(args):
    """Parse serialized shell-compatible args string

    Parse a string produced by shell_encode().
    """

    args = args.replace('x', '=')
    argv = json.loads(base64.b32decode(args).decode('ascii'))
    return [str(arg) for arg in argv]

def execute(args):
    """Custom version of the os.exec* functions

    We can't use os.exec since on Windows, that spawns a new cmd.exe shell and
    doesn't transfer $PATH. Sigh.
    """

    args = [str(arg) for arg in args]
    sys.exit(subprocess.call(args))

def main():
    program_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
    if sys.argv[1:2] != ['--rerun-in-venv']:
        if sys.argv[1:]:
            venv = sys.argv[1]
        else:
            venv = os.path.join(program_dir, '..', 'venv')
        init_sh_bat = os.path.join(program_dir, 'init_shell_helper.bat')
        args_b32 = shell_encode(sys.argv[2:])
        already_in_venv = os.environ.get('VIRTUAL_ENV')
        do_deactivate = 'deactivate' if already_in_venv else ''
        execute([init_sh_bat, do_deactivate, venv,
                 "python", sys.argv[0], '--rerun-in-venv', args_b32])
    else:
        # We are within the venv now. Decode args and run client programs.
        args = shell_decode(sys.argv[2])
        if not args:
            args = ["cmd.exe"]
        print("Running", repr(args), file=sys.stderr)
        execute(args)

if __name__ == '__main__':
    main()
