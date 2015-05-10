import sys
import os
import copy

import pytest

import pymaplib

DATA_DIR = os.path.join(os.path.dirname(__file__), 'data')
NO_GEOINFO_TIF = os.path.join(DATA_DIR, 'no-geoinfo.tif')

def test_copy_handler():
    m = pymaplib.LoadMap(NO_GEOINFO_TIF)
    with pytest.raises(TypeError):
        copy.copy(m)
    with pytest.raises(TypeError):
        copy.copy(m.get())

    c = pymaplib.MapPixelCoord(10, 10)
    cc = copy.copy(c)
    c.x = 20
    assert cc.x == 10
    assert cc.y == 10

    cc.y = 40
    assert c.x == 20
    assert c.y == 10
