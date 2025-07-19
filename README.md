# Viking ![Build Status](https://github.com/viking-gps/viking/actions/workflows/build.yml/badge.svg) ![Variants](https://github.com/viking-gps/viking/actions/workflows/build-variants.yml/badge.svg)
[![GitHub license](https://img.shields.io/github/license/viking-gps/viking)](https://github.com/viking-gps/viking/master/COPYING)

Viking is a free/open source program to manage GPS data. You can
import, plot and create tracks, routes and waypoints, show OSM
and other maps, generate maps (using Mapnik),
see real-time GPS position, Geotag Images,
control items, upload/download OSM Traces and more.
It is written mainly in C with some C++ and uses the GTK+3 toolkit.

## Support

Primary website:
https://viking.sf.net/

About:
https://sourceforge.net/p/viking/wikiallura/Main_Page/

Forums:
https://sourceforge.net/p/viking/discussion/general/

Other:
https://github.com/viking-gps/viking/issues

https://www.reddit.com/r/viking_gps/

## Obtaining Viking

You can download tarball of latest released version at
https://sourceforge.net/projects/viking/files

You can also retrieve the latest development version on the official
Git repository:

	$ git clone git://git.code.sf.net/p/viking/code viking

## Installing Viking

### Dependencies

On Debian Sid, the following packages must be installed before building:

	$ sudo apt install gcc g++ make gtk-doc-tools docbook-xsl yelp-tools libpng-dev libgtk-3-dev libicu-dev libjson-glib-dev intltool autopoint xxd

The following packages are needed (they are included by default in Debian Sid, but not in other distributions). They must be installed too:

	$ sudo apt-get install libcurl4-gnutls-dev libglib2.0-dev-bin librest-dev

The following packages are also used, but they can each be disabled with configure options, if desired:

	$ sudo apt-get install libsqlite3-dev nettle-dev libmapnik-dev libgeoclue-2-dev libgexiv2-dev libgps-dev libmagic-dev libbz2-dev libzip-dev liboauth-dev libnova-dev liblzma-dev

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

In order to ease the creation of a development environment, there is a [development container]() description in `.devcontainer/devcontainer.json`.
This file can be used in different tools, like [Visual Studio Code](https://code.visualstudio.com/docs/devcontainers/tutorial) or to create online environments [Github Codespaces](https://docs.github.com/en/codespaces/setting-up-your-project-for-codespaces/adding-a-dev-container-configuration/introduction-to-dev-containers).

### In-container development with Visual Studio Code on Windows

On Windows, install an X server (such as [VcXsrv](https://vcxsrv.sourceforge.net)) and run it **disabling the access control**.

Make sure Docker is running.

Run Visual Studio Code and "open in container" the directory containing the Viking's source code.

You should now be able to compile, install and run Viking from Visual Studio Code's terminal; the application is displayed as a normal window, through the X server.
