#!/usr/bin/env python
#
# Inspired by MBUtils:
#  http://github.com/mapbox/mbutil
#
# Licensed under BSD
#
import sqlite3, sys, logging, time, os, re

from optparse import OptionParser

logger = logging.getLogger(__name__)

#
# Functions from mbutil for sqlite DB format and connections
#  utils.py:
#
def flip_y(zoom, y):
    return (2**zoom-1) - y

def mbtiles_setup(cur):
    cur.execute("""
        create table tiles (
            zoom_level integer,
            tile_column integer,
            tile_row integer,
            tile_data blob);
            """)
    cur.execute("""create table metadata
        (name text, value text);""")
    cur.execute("""create unique index name on metadata (name);""")
    cur.execute("""create unique index tile_index on tiles
        (zoom_level, tile_column, tile_row);""")

def mbtiles_connect(mbtiles_file):
    try:
        con = sqlite3.connect(mbtiles_file)
        return con
    except Exception, e:
        logger.error("Could not connect to database")
        logger.exception(e)
        sys.exit(1)

def optimize_connection(cur):
    cur.execute("""PRAGMA synchronous=0""")
    cur.execute("""PRAGMA locking_mode=EXCLUSIVE""")
    cur.execute("""PRAGMA journal_mode=DELETE""")

def write_database(cur):
    logger.debug('analyzing db')
    cur.execute("""ANALYZE;""")

def optimize_database(cur):
    logger.debug('cleaning db')
    cur.execute("""VACUUM;""")

#
# End functions from mbutils
#

# Based on disk_to_mbtiles in mbutil
def vikcache_to_mbtiles(directory_path, mbtiles_file, **kwargs):
    logger.debug("%s --> %s" % (directory_path, mbtiles_file))
    con = mbtiles_connect(mbtiles_file)
    cur = con.cursor()
    optimize_connection(cur)
    mbtiles_setup(cur)
    image_format = 'png'
    count = 0
    start_time = time.time()
    msg = ""

    #print ('tileid ' + kwargs.get('tileid'))
    # Need to split tDddsDdzD
    #  note zoom level can be negative hence the '-?' term
    p = re.compile ('^t'+kwargs.get('tileid')+'s(-?\d+)z\d+$')
    for ff in os.listdir(directory_path):
        # Find only dirs related to this tileset
        m = p.match(ff);
        if m:
            s = p.split(ff)
            if len(s) > 2:
                #print s[1]
                # For some reason Viking does '17-zoom level' - so need to reverse that
                z = 17 - int(s[1])
                #print z
                for r2, xs, ignore in os.walk(os.path.join(directory_path, ff)):
                    for x in xs:
                        #print('x:'+directory_path+'/'+ff+'/'+x)
                        for r3, ignore, ys in os.walk(os.path.join(directory_path, ff, x)):
                            for y in ys:
                                #print('tile:'+directory_path+'/'+ff+'/'+x+'/'+y)
                                # Sometimes have random tmp files left around so skip over these
                                if "tmp" in y.lower():
                                    continue
                                f = open(os.path.join(directory_path, ff, x, y), 'rb')
                                # Viking in xyz so always flip
                                y = flip_y(int(z), int(y))
                                cur.execute("""insert into tiles (zoom_level,
                                             tile_column, tile_row, tile_data) values
                                             (?, ?, ?, ?);""",
                                            (z, x, y, sqlite3.Binary(f.read())))
                                f.close()
                                count = count + 1
                                if (count % 100) == 0:
                                    for c in msg: sys.stdout.write(chr(8))
                                    msg = "%s tiles inserted (%d tiles/sec)" % (count, count / (time.time() - start_time))
                                    sys.stdout.write(msg)

    msg = "\nTotal tiles inserted %s \n" %(count)
    sys.stdout.write(msg)
    write_database(cur)
    if not kwargs.get('nooptimize'):
        sys.stdout.write("Optimizing...\n")
        optimize_database(con)
    return

##
## Start of code here
##
parser = OptionParser(usage="""usage: %prog [options] in-map-cache-directory-root  out-file.mbtile
    
Example:
    
Export Viking's cache files of a map type to an mbtiles file:
$ viking-cache-mbtile.py -t 17 ~/.viking-maps OSM_Cycle.mbtiles
    
Import from an MB Tiles file into Viking's cache file layout is not available [yet]

Note you can use the http://github.com/mapbox/mbutil mbutil script to further handle .mbtiles
such as converting it into an OSM tile layout and then pointing a new Viking Map at that location with the map type of 'On Disk OSM Layout'""")
   
parser.add_option('-t', '--tileid', dest='tileid',
    action="store",
    help='''Tile id of Viking map cache to use (19 if not specified as this is Viking's default (MaqQuest))''',
    type='string',
    default='19')

parser.add_option('-n', '--nooptimize', dest='nooptimize',
    action="store_true",
    help='''Do not attempt to optimize the mbtiles output file''',
    default=False)

(options, args) = parser.parse_args()

if len(args) != 2:
    parser.print_help()
    sys.exit(1)

if not os.path.isdir(args[0]):
    sys.stderr.write('Viking Map Cache directory not specified\n')
    sys.exit(1)

if os.path.isfile(args[1]):
    sys.stderr.write('Output file already exists!\n')
    sys.exit(1)

# to mbtiles
if os.path.isdir(args[0]) and not os.path.isfile(args[0]):
    directory_path, mbtiles_file = args
    vikcache_to_mbtiles(directory_path, mbtiles_file, **options.__dict__)
