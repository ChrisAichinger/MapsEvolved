import contextlib

# Load all the SIP wrappers into here
from . import maplib_sip as maplib_sip
from .maplib_sip import *


class DefaultPersistentStore:
    """The default PersistenStore wrapped in a Python context manager"""

    @staticmethod
    @contextlib.contextmanager
    def Read():
        ps = maplib_sip.CreatePersistentStore()
        ps.OpenRead()
        try:
            yield ps
        finally:
            ps.Close()

    @staticmethod
    @contextlib.contextmanager
    def Write():
        ps = maplib_sip.CreatePersistentStore()
        ps.OpenWrite()
        try:
            yield ps
        finally:
            ps.Close()
