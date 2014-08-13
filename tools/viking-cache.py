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

def cache_converter_to_osm (vc_path, target_path, **kwargs):
    msg = ''
    count = 0
    onlydigits_re = re.compile ('^\d+$')
    etag_re = re.compile ('\.etag$')
    path_re = re.compile ('^t'+kwargs.get('tileid')+'s(-?\d+)z\d+$')

    if not os.path.isdir(target_path):
        os.makedirs(target_path)

    start_time = time.time()
    for ff in os.listdir(vc_path):
        # Find only dirs related to this tileset
        m = path_re.match(ff);
        if m:
            s = path_re.split(ff)
            if len(s) > 2:
                #print (s[1])
                # For some reason Viking does '17-zoom level' - so need to reverse that
                z = 17 - int(s[1])
                tile_dirz = os.path.join(target_path, str(z))

                if not os.path.isdir(tile_dirz):
                    #print (os.path.join(vc_path, ff) +":"+ tile_dirz)
                    os.rename(os.path.join(vc_path, ff), tile_dirz)

                for r2, xs, ignore in os.walk(tile_dirz):
                    for x in xs:
                        tile_dirx = os.path.join(tile_dirz, str(x))

                        # No need to move X dir

                        for r3, ignore, ys in os.walk(tile_dirx):
                            for y in ys:
                                m2 = onlydigits_re.match(y);
                                if m2:
                                    # Move and append extension to everything else
                                    # OSM also in flipped y, so no need to change y

                                    # Only overwrite existing tile if specified
                                    target_tile = os.path.join(tile_dirx, y + ".png")
                                    if not os.path.isfile(target_tile) or kwargs.get('force'):
                                        os.rename(os.path.join(tile_dirx, y), target_tile)

                                    count = count + 1
                                    if (count % 100) == 0:
                                        for c in msg: sys.stdout.write(chr(8))
                                        msg = "%s tiles moved (%d tiles/sec)" % (count, count / (time.time() - start_time))
                                        sys.stdout.write(msg)
                                else:
                                    # Also rename etag files appropriately
                                    m3 = etag_re.search(y);
                                    if m3:
                                        target_etag = y.replace (".etag", ".png.etag")
                                        if not os.path.isfile(os.path.join(tile_dirx,target_etag)) or kwargs.get('force'):
                                            os.rename(os.path.join(tile_dirx, y), os.path.join(tile_dirx, target_etag))
                                    else:
                                        # Ignore all other files
                                        continue

    msg = "\nTotal tiles moved %s \n" %(count)
    sys.stdout.write(msg)
    return

#
# Mainly for testing usage.
# Don't expect many people would want to convert back to the old layout
#
def cache_converter_to_viking (osm_path, target_path, **kwargs):
    msg = ''
    count = 0
    ispng = re.compile ('\.png$')

    if not os.path.isdir(target_path):
        os.makedirs(target_path)

    start_time = time.time()
    for r1, zs, ignore in os.walk(osm_path):
        for z in zs:
            # For some reason Viking does '17-zoom level' - so need to reverse that
            vz = 17 - int(z)
            tile_dirz = os.path.join(target_path, 't'+ kwargs.get('tileid') + 's' + str(vz) + 'z0')

            if not os.path.isdir(tile_dirz):
                os.rename(os.path.join(osm_path, z), tile_dirz)

            for r2, xs, ignore in os.walk(tile_dirz):
                for x in xs:

                    tile_dirx = os.path.join(tile_dirz, x)
                    # No need to move X dir

                    for r3, ignore, ys in os.walk(tile_dirx):
                        for y in ys:
                            m = ispng.search(y);
                            if m:
                                # Move and remove extension to everything else
                                # OSM also in flipped y, so no need to change y

                                # Only overwrite existing tile if specified
                                y_noext = y
                                y_noext = y_noext.replace (".png", "")
                                target_tile = os.path.join(tile_dirx, y_noext)
                                if not os.path.isfile(target_tile) or kwargs.get('force'):
                                    os.rename(os.path.join(tile_dirx, y), target_tile)

                                count = count + 1
                                if (count % 100) == 0:
                                    for c in msg: sys.stdout.write(chr(8))
                                    msg = "%s tiles moved (%d tiles/sec)" % (count, count / (time.time() - start_time))
                                    sys.stdout.write(msg)

    msg = "\nTotal tiles moved %s \n" %(count)
    sys.stdout.write(msg)
    return

def get_tile_path (tid):
    # Built in Tile Ids
    tile_id = int(tid)
    if tile_id == 13:
        return "OSM-Mapnik"
    elif tile_id == 15:
      return "BlueMarble"
    elif tile_id == 17:
        return "OSM-Cyle"
    elif tile_id == 19:
        return "OSM-MapQuest"
    elif tile_id == 21:
        return "OSM-Transport"
    elif tile_id == 22:
        return "OSM-Humanitarian"
    elif tile_id == 212:
        return "Bing-Aerial"
    # Default extension Map ids (from data/maps.xml)
    elif tile_id == 29:
        return "CalTopo"
    elif tile_id == 101:
        return "pnvkarte"
    elif tile_id == 600:
        return "OpenSeaMap"
    else:
        return "unknown"

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

Convert from Viking's Legacy cache format to the more standard OSM layout style for a built in map type:
$ ./viking-cache.py -m vlc2osm -t 13 -f ~/.viking-maps ~/.viking-maps
Here the tiles get automatically moved to ~/.viking-maps/OSM-Mapnik

Correspondingly change the Map layer property to use OSM style cache layout in Viking.

Convert from Viking's Legacy cache format to the more standard OSM layout style for a extension map type:
$ ./viking-cache.py -m vlc2osm -t 110 -f ~/.viking-maps ~/.viking-maps/StamenWaterColour
Here one must specify the output directory name explicitly and set your maps.xml file with the name=StamenWaterColour for the id=110 entry
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
    else:
        if options.__dict__.get('mode') == 'vlc2osm':
            # Main forward conversion
            is_default_re = re.compile ("\.viking-maps\/?$")
            out_fd2 = is_default_re.search(out_fd)
            if out_fd2:
                # Auto append default tile name to the path
                tile_path = get_tile_path(options.__dict__.get('tileid'))
                if tile_path == "unknown":
                    sys.stderr.write ("Could not convert tile id to a name")
                    sys.stderr.write ("Specifically set the output directory to something other than the default")
                    sys.exit(2)
                else:
                    print ("Using tile name %s" %(tile_path) )
                    out_fd2 = os.path.join(out_fd, tile_path)
            else:
                out_fd2 = out_fd

            if os.path.isdir(args[0]):
                cache_converter_to_osm(in_fd, out_fd2, **options.__dict__)
        else:
            if options.__dict__.get('mode') == 'osm2vlc':
                # Convert back if needs be
                if os.path.isdir(args[0]):
                    cache_converter_to_viking(in_fd, out_fd, **options.__dict__)
