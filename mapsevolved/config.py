
import contextlib

WINREG_KEY = "Software\\OutdoorMapper"

winreg = None
try:
    import winreg
except ImportError:
    pass

if winreg:
    class Config:
        def __init__(self, mode):
            mode_map = { 'r': winreg.KEY_READ,
                         'w': winreg.KEY_WRITE }
            if mode not in mode_map:
                raise ValueError("Invalid mode %r", mode)
            mode = mode_map[mode]
            self.key = winreg.OpenKey(winreg.HKEY_CURRENT_USER,
                                      WINREG_KEY, access=mode)
        def close(self):
            self.key.Close()

        def _get_value(self, keyname, requested_type):
            try:
                value, found_type = winreg.QueryValueEx(self.key, keyname)
            except OSError as e:
                raise KeyError("Config entry not found: %r", keyname) from e

            if requested_type != found_type:
                raise KeyError("Config entry has the wrong type: %r", keyname)

            return value

        def _set_value(self, keyname, value_type, value):
            try:
                winreg.SetValueEx(self.key, keyname, None, value_type, value)
            except OSError as e:
                raise KeyError("Couldn't write config entry: %r\n%r",
                               keyname, value) from e

        def get_stringlist(self, keyname):
            return self._get_value(keyname, winreg.REG_MULTI_SZ)

        def set_stringlist(self, keyname, value):
            return self._set_value(keyname, winreg.REG_MULTI_SZ, value)

        def get_string(self, keyname):
            return self._get_value(keyname, winreg.REG_SZ)

        def set_string(self, keyname, value):
            return self._set_value(keyname, winreg.REG_SZ, value)

        def get_int(self, keyname):
            return self._get_value(keyname, winreg.REG_DWORD)

        def set_int(self, keyname, value):
            return self._set_value(keyname, winreg.REG_DWORD, value)

        @staticmethod
        @contextlib.contextmanager
        def read():
            c = Config('r')
            try:
                yield c
            finally:
                c.close()

        @staticmethod
        @contextlib.contextmanager
        def write():
            c = Config('w')
            try:
                yield c
            finally:
                c.close()
