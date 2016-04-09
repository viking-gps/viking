#! /usr/bin/perl -w
# Copyright (C) 2010 Rob Norris <rw_norris@hotmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#


=head1 Overview

A script to auto generate basic Viking .vik files for directories containing images.
Note that from Viking 1.3 onwards it can load geotagged images directly,
 although it does not have recursive directory capabilities.

Simply recursively search down the directory tree (from the current location) for suitable image files
 [normally jpg|JPG (probably photographs)] and then extract any location data from the EXIF part.

For each directory this info is output to a file into either Viking (default) or GPX data file formats.
Output filename is waypoints.vik (or waypoints.gpx in GPX mode) unless the -o option is specified.


Options:
-g put into outputting GPX file mode
-o <name> - specify output base filename (overriding 'waypoints')
-r make waypoint image filenames relative (rather than absolute)

Required programs:
. exiftool - getting location data from EXIF (Debian package libimage-exiftool-perl)

Various improvements can be:
. Command line options to control things eg:
    .which symbol to use for each point
    .which Viking Map / other viking defaults
    .a non recursive mode
    .a mode to generate one massive file instead of one per directory 

. Work out zoom factor to see all points in Viking
. Metadata bounds for gpx

. Any Speed optimizations - deciding which files to process could be improved
. Maybe better control of which files to analyse - maybe any - not just files named .jpg
. Is even doing this in Perl the best tool for the job [consider python, C, etc...]?

=cut 

# ************ START OF CODE *******************

use strict;
use File::Find;
use Image::ExifTool qw(:Public);

# Output modes
use constant VIKING => 1;
use constant GPX => 2;

use constant RELATIVE => 1;
use constant ABSOLUTE => 2;

#################################
# Some global variables
# Create a new Image::ExifTool object
my $exifTool = new Image::ExifTool;

my @waypoint = ("","","","","","");
my @position = ("0.0", "0.0"); # lat / long

my $out_file;

# Default mode
my $mode = VIKING;
my $imagefilemode = ABSOLUTE;

#################################

# Write header first part of .vik file
sub Header_Viking {

    return <<END;
#VIKING GPS Data file http://viking.sf.net/

~Layer Map
name=Map
mode=13
directory=
alpha=255
autodownload=f
mapzoom=0
~EndLayer


~Layer TrackWaypoint
name=TrackWaypoint
tracks_visible=f
waypoints_visible=t
routes_visible=f
drawmode=0
drawlines=t
drawpoints=t
drawelevation=f
elevation_factor=30
drawstops=f
stop_length=60
line_thickness=1
bg_line_thickness=0
trackbgcolor=#ffffff
drawlabels=t
wpcolor=#000000
wptextcolor=#ffffff
wpbgcolor=#8383c4
wpbgand=t
wpsymbol=0
wpsize=4
wpsyms=t
drawimages=t
image_size=64
image_alpha=255
image_cache_size=300


~LayerData
type="waypointlist"
END
#
#
}

# Write header first part of .gpx file
sub Header_GPX {
    return <<END;
<?xml version="1.0" encoding="UTF-8"?>
<gpx
 version="1.1"
creator="$0"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
xmlns="http://www.topografix.com/GPX/1/1"
xsi:schemaLocation="http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd">
END
# TODO consider metadata time & bounds info
}

sub Footer_GPX {
    return "</gpx>\n";
}

sub My_Process_File {
    my ($dir, $file) = @_;

    ## Start
    ##

    # Only do files we are interested in
    unless ($file =~ m/\.(?:JPG|jpg)$/) {
	return;
    }
    #print "My_Process_File $file\n";

    @waypoint = ("","","","","","");

    # Extract meta information from an image
    my $info = $exifTool->ImageInfo("$dir/$file");

    #DateTimeOriginal
    #GPSVersionID
    #GPSAltitude
    #GPSLatitude (1)
    #GPSLongitude (1)

    foreach (sort keys %$info) {
	#print "$file: $_ => $$info{$_}\n";
	
	# If we can make the sort in reverse 

	#if (/GPSVersionID/) {
	#    unless ("$$info{$_}" eq "2.0.0.0") {
	#	# Only handle version 2 Ids
	#	return;
	#    }
	#}

	# Assume datum is in WGS-84
	if (/^GPSLatitude\ /) {
	    $waypoint[0] = $$info{$_};
	    next;
	}

	if (/^GPSLatitudeRef/) {
	    $waypoint[1] = $$info{$_};
	    next;
	}

	if (/^GPSLongitude\ /) {
	    $waypoint[2] = $$info{$_};
	    next;
	}

	if (/^GPSLongitudeRef/) {
	    $waypoint[3] = $$info{$_};
	    next;
	}

	if (/^GPSAltitude\ /) {
	    my @row = split / /, $$info{$_};
	    $waypoint[4] = $row[0]; #hopefully in metres
	    next;
	}

	if (/^DateTimeOriginal$/) {
	    $waypoint[5] = $$info{$_};
	    next;
	}

    }

    # Check for South
    if ($waypoint[1] eq "South" && $waypoint[0] ne "") {
	$waypoint[0] = '-'."$waypoint[0]";
    }

    # Check for West
    if ($waypoint[3] eq "West" && $waypoint[2] ne "") {
	$waypoint[2] = '-'."$waypoint[2]";
    }

    # At least lat/long
    if ($waypoint[0] ne "" && $waypoint[2] ne "") {
	# Update position so can track where to center map
	$position[0] = $waypoint[0];
	$position[1] = $waypoint[2];
	my ($name, $path) = File::Basename::fileparse ($file, qr/\.[^.]*/);
	my $filename = "$file"; # Relative filename
	if ($mode == VIKING) {
	    # ATM, Viking (0.9.94) wp image loading only works if absolute
	    # See SF 2998555
	    # Create absolute filename
	    if ($imagefilemode == ABSOLUTE) {
		my ($abs_path) = File::Spec->rel2abs("$path", "$dir");
		$filename = "$abs_path/$file";
	    }
	    return "type=\"waypoint\" latitude=\"$waypoint[0]\" longitude=\"$waypoint[2]\" name=\"$name\" altitude=\"$waypoint[4]\" comment=\"$waypoint[5]\" image=\"$filename\" symbol=\"scenic area\"\n";
	}
	else {
	    return "<wpt lat=\"$waypoint[0]\" lon=\"$waypoint[2]\">\n  <name>$name</name>\n  <ele>$waypoint[4]</ele>\n  <desc>$waypoint[5]</desc>\n  <sym>scenic area</sym>\n</wpt>";
	}

    }

    return "";
    ## END
    ##
}

sub Footer_Viking {
	return <<END;
type="waypointlistend"
~EndLayerData
~EndLayer

xmpp=32.000000
ympp=32.000000
lat=$position[0]
lon=$position[1]
mode=mercator
color=#cccccc
drawscale=t
drawcentermark=t

END
}

#
sub My_Process_Dir {
    my ($dir) = @_;
    my $FTT = 1; # First Time Through flag to mark first pass
    my $has_location = 0; 

    #print "My_Process_Dir $dir\n";
    opendir(DIR, $dir) or die "$0: can't opendir $dir: $!\n";
    
    my $line;
    while (defined(my $file = readdir(DIR))) {
	next if $file =~ /^\.\.?$/;     # skip . and ..
	$line = My_Process_File($dir, $file);
	# At least one file with location data exists
	if (defined ($line)) {
	    if ($line ne "" && $FTT == 1) {
		$FTT = 0;
		open (FILE, ">$dir/$out_file") or die "$0: Can not open $!\n";

		if ($mode == VIKING) {
		    print FILE Header_Viking();
		}
		else {
		    print FILE Header_GPX();
		}
		$has_location = 1; # Remember that we have found something
	    }
	    if ($line ne "") {
		print FILE $line;
	    }
	}
    }

    if ($has_location) {
	if ($mode == VIKING) {
	    print FILE Footer_Viking();
	}
	else {
	    print FILE Footer_GPX();
	}
	close (FILE);
    }
    closedir(DIR);
}

#
sub Process_File {
    my $file = $_;
    unless (-d $file) {
	return;
    }    
    return if $file =~ /^\.\.$/; # Ensure skip ..
    print "$0: Doing directory $file\n";
    My_Process_Dir($file);
}

############ START ##################

# Default filename
my $out_file_start = "waypoints";

if (@ARGV) {
    for (my $arg=0; $arg < $#ARGV+1; $arg++) {
	if ("$ARGV[$arg]" eq "-o") {
	    # Set filename to next arg
	    if ($arg < $#ARGV) {
		$out_file_start = $ARGV[$arg + 1];
	    }
	}
	if ("$ARGV[$arg]" eq "-g") {
	    $mode = GPX;
	}
	if ("$ARGV[$arg]" eq "-r") {
	    $imagefilemode = RELATIVE;
	}
    }
}
#print "$0: Mode is $mode\n";

if ($mode == VIKING) {
    $out_file = "$out_file_start".".vik";
}
else {
    $out_file = "$out_file_start".".gpx";
}
#print "$0: File output is $out_file\n";

$exifTool->Options(CoordFormat => q{%.6f});
$exifTool->Options(FastScan => 1);

# Only get information in Standard EXIF
$exifTool->Options(Group0 => ['EXIF']);

find(\&Process_File, ".");

#end
