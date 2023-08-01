# Viking [![Build Status](https://travis-ci.com/viking-gps/viking.svg?branch=master)](https://travis-ci.com/viking-gps/viking)
[![GitHub license](https://img.shields.io/github/license/viking-gps/viking)](https://github.com/viking-gps/viking/master/COPYING)

[![Build history](https://buildstats.info/travisci/chart/viking-gps/viking?branch=master&showStats=true)](https://travis-ci.com/viking-gps/viking/builds)

Viking is a free/open source program to manage GPS data. You can
import, plot and create tracks, routes and waypoints, show OSM
and other maps, generate maps (using Mapnik),
see real-time GPS position, Geotag Images,
control items, upload/download OSM Traces and more.
It is written mainly in C with some C++ and uses the GTK+3 toolkit.

Website: http://viking.sf.net/


## Obtaining Viking

You can download tarball of latest released version at
https://sourceforge.net/projects/viking/files

You can also retrieve the latest development version on the official
Git repository:

	$ git clone git://git.code.sf.net/p/viking/code viking

## Installing Viking

### Dependencies

On Debian Sid, following packages must be installed before building:

	$ sudo apt install gtk-doc-tools docbook-xsl yelp-tools libpng-dev libgtk-3-dev libicu-dev libjson-glib-dev intltool xxd

The following packages are needed (they are included by default in Debian Sid, but not in other distributions). They must be installed too:

	$ sudo apt-get install libcurl4-gnutls-dev libglib2.0-dev-bin

The following packages are also used, but they can each be disabled with configure options, if desired:

	$ sudo apt-get install libsqlite3-dev nettle-dev libmapnik-dev libgeoclue-2-dev libgexiv2-dev libgps-dev libmagic-dev libbz2-dev libzip-dev liboauth-dev libnova-dev

### Actual Build

If you downloaded Viking from Git, you have to:

	$ ./autogen.sh

Next, or if you downloaded a tarball, you have to:

	$ ./configure
	$ make

Check output of "./configure --help" for configuration options.  In
particular, it is possible to disable some features, like
--disable-google in order to disable any Google stuff.

If you wish to install Viking, you have to (as root):

	# make install

For detailed explanation on the install on Unix like systems,
see the INSTALL file.

### Examples

See test/ subdirectory for examples.

## Documentation

See doc/ and help/ subdirectories for documentation.
You can also access user manual via Help menu entry.

## Contributing

First, read [hacking notes](HACKING).

In order to ease the creation of a development environement, there is a [development container]() description in `.devcontainer/devcontainer.json`.
This file can be use in different tools, like [Visual Studio Code](https://code.visualstudio.com/docs/devcontainers/tutorial) or to create online environements [Github Codespaces](https://docs.github.com/en/codespaces/setting-up-your-project-for-codespaces/adding-a-dev-container-configuration/introduction-to-dev-containers).