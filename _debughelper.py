import sys
import os
import pymaplib

with pymaplib.DefaultPersistentStore.Read() as ps:
    maplist = ps.GetStringList('maps')

rmc = pymaplib.RasterMapCollection()
for mapfile in maplist:
    pymaplib.LoadMap(rmc, mapfile)

m1 = rmc.Get(0)
m2 = rmc.Get(0)
p1 = m1.GetProj()
