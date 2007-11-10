#!/usr/bin/env python

# WARNING: I haven't thought much about the best way of doing
#	   this. it may get lots of extra caches, or take a long
#	   time if your path goes thru a city.
# there's probably a better algorithm.

# in meters.
maxdist = 2500.0

# get geocaches, circles of "radius", then go ahead advance_dist in the track and do it again.
#radius = maxdist * 2 * 0.707106 # sqrt(2)
radius = maxdist * 2
advance_dist = maxdist * 2


# TODO: shell escape in case mkdtemp has unsafe chars in (unlikely)

class Coord:
  def __init__(self, lat=0.0, lon=0.0):
    self.lat = lat
    self.lon = lon

  # diff between two coords in meters.
  def diff(c1,c2):
    from math import acos, sin, cos
    PIOVER180 = 3.14159265358979 / 180.0
    EquatorialRadius = 6378137 # WGS-84
    lat1 = c1.lat * PIOVER180
    lon1 = c1.lon * PIOVER180
    lat2 = c2.lat * PIOVER180
    lon2 = c2.lon * PIOVER180
    return EquatorialRadius * acos(sin(lat1)*sin(lat2)+cos(lat1)*cos(lat2)*cos(lon1-lon2));

def load_track ( file ):
  import re
  latlonre = re.compile("lat=['\"]([0-9\\-\\.]*?)['\"].*lon=['\"]([0-9\\-\\.]*?)['\"]")
  track = []
  for line in file:
    match = latlonre.search(line)
    if match:
      lat = float(match.group(1))
      lon = float(match.group(2))
      track.append ( Coord(lat,lon) )
  return track

# position inside of a track = index of trackpoint + meters past this trackpoint
class TPos:
  def __init__(self, track):
    self.track = track
    self.n_tps = len(self.track)
    self.i_lasttp = 0 # index of tp before current position
    self.dist_past = 0 # meters past this tp

    self.coord = Coord()

    if self.n_tps > 0:
      self.coord.lat = track[0].lat
      self.coord.lon = track[0].lon

    if self.n_tps > 1:
      self.finished = False
    else:
      self.finished = True # no tps in track, nothing to do

  def recalculate_coord(self):
    if self.i_lasttp >= self.n_tps - 1:
      self.coord = self.track[self.n_tps - 1] # return last tp
      return

    c1 = self.track[self.i_lasttp]
    c2 = self.track[self.i_lasttp+1]

    # APPROXIMATE
    percentage_past = self.dist_past / Coord.diff ( c1, c2 )
    self.coord.lat = c1.lat + percentage_past * (c2.lat - c1.lat)
    self.coord.lon = c1.lon + percentage_past * (c2.lon - c1.lon)

  def advance(self, distance):
    if self.i_lasttp >= (self.n_tps - 1):
      self.finished = True
    if self.finished:
      return

    # possibility one: we don't pass a TP
    dist_to_next_tp = Coord.diff ( self.track[self.i_lasttp], self.track[self.i_lasttp+1] ) - self.dist_past
    if dist_to_next_tp > distance:
      self.dist_past += distance
      self.recalculate_coord()
    else:
      # goto beginning of next tp and try again.
      self.i_lasttp += 1
      self.dist_past = 0

      if self.i_lasttp >= (self.n_tps - 1):
        self.recalculate_coord()
      else:
        self.advance ( distance - dist_to_next_tp )

#    dist_after_pos = dist_after_i
#    if self.track[i_lasttp
# TODO!

  def end(self):
    return self.finished

  
def get_geocaches_for(coord,radius,tmpdir,i):
  import os, sys
  MAXGCS = 100
  radiusinmiles = radius * 100 / 2.54 / 12 / 5280 # meters to miles
  gcgetstr = "gcget %f,%f %d %f > %s/%d.loc" % (coord.lat, coord.lon, MAXGCS, radius, tmpdir, i)
  sys.stderr.write("%s\n" % gcgetstr)
  os.system(gcgetstr)
  #print "type=\"trackpoint\" latitude=\"%f\" longitude=\"%f\"" % (coord.lat, coord.lon)


#------------


import sys, os
tr = load_track ( sys.stdin )
tpos = TPos(tr)

import tempfile
tmpdir = tempfile.mkdtemp()

i = 0
while not tpos.end():
  get_geocaches_for ( tpos.coord, radius, tmpdir, i )
  tpos.advance ( advance_dist )
  i += 1
get_geocaches_for ( tpos.coord, radius, tmpdir, i )

####### condense all #######
gb_input_args = ["-i geo -f %s/%d.loc" % (tmpdir,j) for j in range(i)]
gb_string = "gpsbabel %s -x duplicate,location -o gpx -F -" % (" ".join(gb_input_args))
sys.stderr.write("\n%s\n" % gb_string)
os.system(gb_string)

####### delete temp files #######
for j in range(i):
  os.sys.remove("%s/%d.loc" % (tmpdir,j))
os.sys.rmdir(tmpdir)
