
# User Notes

# Running it

## Flatpak Setup

Ensure the Flatpak system is installed on your Linux system

https://flatpak.org/setup

Also see various other online guides about using Flatpaks if necessary

## Flathub

Install from https://flathub.org/ like many other packages

https://flathub.org/apps/details/org.viking.Viking

## Sourceforge

Alternatively get the _Viking .flatpak_ file from the project Sourceforge website

https://sourceforge.net/projects/viking/files/

Then install the downloaded file e.g. via command line:

    flatpak install --user viking-1.8.flatpak

Confirm download of required runtime(s)

# Run Installed Flatpak

    flatpak run org.viking.Viking

# Flatpak Limitations

Since Viking is running in a sandbox, some system integration features no longer work properly in a Flatpak runtime.

1. Help does not work.
  You may be asked which program to run for the help - normally being _Yelp_, but this is now outside the sandbox and so the reference to the help files doesn't seem to work.

1. Print-Preview does not work.
  Internally GTK seems to try to invoke _evince_, but this is not available in the sandbox and so fails.

1. Realtime tracking via GPSD does not work.
  The sandbox does not allow the libgps component (that connects to GPSD) to open a socket.
  Also its likely that the version GPSD running elsewhere (even if on the host machine) needs to be the same version as the provided libgps for it to work, which is less likely when they are packaged separately.

1. Export Layer --> 'Open with External Program', e.g. normally JOSM or Merkaator - does not work.
  There is no access to these programs as they are outside the sandbox.

1. File --> Acquire --> From GPS... ( or GPS Layer --> Download From GPS )
  This uses GPSBabel in the background to perform the transfer via a Serial Port (in /dev) which is blocked in Flatpak by default.
  Since this mode is not used very often it is not enabled.
  You may want try using the option '--device=all' (this has not been tested) in the command to run Viking e.g.:

    flatpak run --device=all org.viking.Viking


1. Show Picture.
  Although it works; currently you may be asked which application to open the image with every single time, with seemingly no method to remember the selection.

1. Mapnik Layer (Map Creation) is disabled.
  This requires not only a complex build, but also a complex runtime (and general system) setup to be actually useful.
  Also unlikely to be of interest to Flatpak users, so no attempt has been made to facilitate this option.

# Flathub vs Sourceforge Flatpak Differences

## Sourceforge

This includes [GPSBabel](https://www.gpsbabel.org) in the sandbox,
thus various features that use GPSBabel are available.

## Flathub

ATM This does not contain GPSBabel, so features that use GPSBabel do not work (or are completely disabled) including:

1. Import/Export of non built in file types

1. TrackWaypoint Filter functions

1. Serial device transfer

It is a Work in Progress to get a successful automated build of GPSBabel in Flathub due to build dependency on different SDKs
(that can be overcome in the developer build used for the Sourceforge version).

----

# Developer Notes

# Creating the Flatpak

Also see https://docs.flatpak.org/en/latest/index.html

## Building it (locally)

### Install matching runtime & SDK as used in the .yml file e.g.

    flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo

    #flatpak install flathub org.freedesktop.Platform//22.08 org.freedesktop.Sdk//22.08
    flatpak install flathub org.kde.Platform//5.15-22.08 org.kde.Sdk//5.15-22.08

## Build

    #Total clean out in the top level of previous runs:
    #rm -rf .flatpak-builder/ && rm -rf flatpak/ && rm -rf flatpakrepo/
    flatpak-builder flatpak/ org.viking.Viking.yml --force-clean

Clean build takes <1 minute to (re)download sources and about 15 minutes to build (most of it being GTK2) on my machine.
Can rebuild without wiping everything as flatbuilder caches the build, but regularly thinks it doesn't need to rebuild Viking (e.g. not necessarily code changes but build environment things).
Unfortunately I've not discovered how to force a rebuild of just the Viking part, so have to resort to a wipe all (or '--disable-cache').

The org.viking.Viking.yml contains instructions of how to build every dependency used by Viking.
Currently Mapnik bits are (probably) too complicated to bother trying to build,
it's not even been tried in any manner yet as the focus to get a basic version of Viking 1.8 available.
This is something to investigate in the future.

## Install locally

note here for some reason it's '--install' compared to installing a runtime above!

     flatpak-builder --user --install flatpak/ org.viking.Viking.yml --force-clean

## Test it

     flatpak run -v org.viking.Viking

If nothing happens then check any Flatpak things actually work

     flatpak run --devel --command=bash org.freedesktop.Sdk
     echo $?

Not 0?
Run under strace...
exposes bwrap issues!
Fix system

     su -
     sysctl kernel.unprivileged_userns_clone=1
     exit

Still not working on my Debian unstable machine ATM, although OK on Ubuntu!

## Generate .flatpak file

     flatpak-builder --repo=flatpakrepo flatpak/ org.viking.Viking.yml --force-clean
     flatpak build-bundle flatpakrepo/ viking.flatpak org.viking.Viking

Transfer .flatpak file to another machine where flatpak running works and test there.

On version releases, generate the .flatpak file and upload to Sourceforge.


# Flathub Maintainence

https://github.com/flathub/org.viking.Viking

https://github.com/flathub/flathub/wiki/App-Maintenance

## Prodecure

Once new release of Viking is made (or indeed as part of creating the release - a local flatpak test - as outlined above - should resolve what is needed for the .yml / appdata.xml changes).
Thus it should be straight-forward to update the flathub .yml file to point to the new git tag, including any runtime changes/updates/improvements.
