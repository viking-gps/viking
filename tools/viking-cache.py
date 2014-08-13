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
    except Exception as e:
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
    onlydigits_re = re.compile ('^\d+$')

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
                #print (s[1])
                # For some reason Viking does '17-zoom level' - so need to reverse that
                z = 17 - int(s[1])
                #print (z)
                for r2, xs, ignore in os.walk(os.path.join(directory_path, ff)):
                    for x in xs:
                        # Try to ignore any non cache directories
                        m2 = onlydigits_re.match(x);
                        if m2:
                            #print('x:'+directory_path+'/'+ff+'/'+x)
                            for r3, ignore, ys in os.walk(os.path.join(directory_path, ff, x)):
                                for y in ys:
                                    # Legacy viking cache file names only made from digits
                                    m3 = onlydigits_re.match(y);
                                    if m3:
                                        #print('tile:'+directory_path+'/'+ff+'/'+x+'/'+y)
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
    if count == 0:
        print ("No tiles inserted. NB This method only works with the Legacy Viking cache layout")
    else:
        write_database(cur)
        if not kwargs.get('nooptimize'):
            sys.stdout.write("Optimizing...\n")
            optimize_database(con)
    return

def mbtiles_to_vikcache(mbtiles_file, directory_path, **kwargs):
    logger.debug("Exporting MBTiles to disk")
    logger.debug("%s --> %s" % (mbtiles_file, directory_path))
    con = mbtiles_connect(mbtiles_file)
    count = con.execute('select count(zoom_level) from tiles;').fetchone()[0]
    done = 0
    msg = ''
    base_path = directory_path
    if not os.path.isdir(base_path):
        os.makedirs(base_path)

    start_time = time.time()

    tiles = con.execute('select zoom_level, tile_column, tile_row, tile_data from tiles;')
    t = tiles.fetchone()
    while t:
        z = t[0]
        x = t[1]
        y = t[2]
        # Viking in xyz so always flip
        y = flip_y(int(z), int(y))
        # For some reason Viking does '17-zoom level' - so need to reverse that
        vz = 17 - int(t[0])
        tile_dir = os.path.join(base_path, 't'+ kwargs.get('tileid') + 's' + str(vz) + 'z0', str(x))
        if not os.path.isdir(tile_dir):
            os.makedirs(tile_dir)
        # NB no extension for VikCache files
        tile = os.path.join(tile_dir,'%s' % (y))
        # Only overwrite existing tile if specified
        if not os.path.isfile(tile) or kwargs.get('force'):
            f = open(tile, 'wb')
            f.write(t[3])
            f.close()
        done = done + 1
        if (done % 100) == 0:
            for c in msg: sys.stdout.write(chr(8))
            msg = "%s / %s tiles imported (%d tiles/sec)" % (done, count, done / (time.time() - start_time))
            sys.stdout.write(msg)
        t = tiles.fetchone()
    msg = "\nTotal tiles imported %s \n" %(done)
    sys.stdout.write(msg)
    return

##
## Start of code here
##
parser = OptionParser(usage="""usage: %prog -m <mode> [options] input output

When either the input or output refers to a Viking legacy cache ('vcl'), is it the root directory of the cache, typically ~/.viking-maps

Examples:

Export Viking's legacy cache files of a map type to an mbtiles file:
$ ./viking-cache.py -m vlc2mbtiles -t 17 ~/.viking-maps OSM_Cycle.mbtiles

Note you can use the http://github.com/mapbox/mbutil mbutil script to further handle .mbtiles
such as converting it into an OSM tile layout and then pointing a new Viking Map at that location with the map type of 'On Disk OSM Layout'

Import from an MB Tiles file into Viking's legacy cache file layout, forcing overwrite of existing tiles:
$ ./viking-cache.py -m mbtiles2vlc -t 321 -f world.mbtiles ~/.viking-maps
NB: You'll need to a have a corresponding ~/.viking/maps.xml definition for the tileset id when it is not a built in id
""")
   
parser.add_option('-t', '--tileid', dest='tileid',
    action="store",
    help='''Tile id of Viking map cache to use (19 if not specified as this is Viking's default (MaqQuest))''',
    type='string',
    default='19')

parser.add_option('-n', '--nooptimize', dest='nooptimize',
    action="store_true",
    help='''Do not attempt to optimize the mbtiles output file''',
    default=False)

parser.add_option('-f', '--force', dest='force',
    action="store_true",
    help='''Force overwrite of existing tiles''',
    default=False)

parser.add_option('-m', '--mode', dest='mode',
    action="store",
    help='''Mode of operation which must be specified. "vlc2mbtiles", "mbtiles2vlc", "vlc2osm", "osm2vlc"''',
    type='string',
    default='none')

(options, args) = parser.parse_args()

if options.__dict__.get('mode') ==  'none':
    sys.stderr.write ("\nError: Mode not specified\n")
    parser.print_help()
    sys.exit(1)

if len(args) != 2:
    parser.print_help()
    sys.exit(1)

in_fd, out_fd = args

if options.__dict__.get('mode') == 'vlc2mbtiles':
    # to mbtiles
    if os.path.isdir(args[0]) and not os.path.isfile(args[0]):
        vikcache_to_mbtiles(in_fd, out_fd, **options.__dict__)
else:
    if options.__dict__.get('mode') == 'mbtiles2vlc':
        # to VikCache
        if os.path.isfile(args[0]):
            mbtiles_to_vikcache(in_fd, out_fd, **options.__dict__)

