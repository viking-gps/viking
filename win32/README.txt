
The build under Windows follows the same process as the traditional Linux build:

    cd ../src
    sh configure <options>
    make

There is a separate installer method using NSIS to generate the viking-1.X.Y.Z.exe installer file.

To generate the installer (once the code has been compiled as above) run the installer.bat

For further details (especially about how to set up the development dependencies) see:
http://sourceforge.net/apps/mediawiki/viking/index.php?title=WindowsBuildInstructions

These instructions have now been turned turned into a series of scripts to produce the Viking installer:

1. prepare.bat
2a. configure_and_make.bat
2b. make.bat
3. installer.bat

The scripts are aimed at a pristine Windows XP installation.
Using *prepare.bat* will attempt to *download* and *install* various Open Source software to aid this process.
An administrator level account is needed to run this which will *modify* your system.

The other scripts simply make use of these (assumed) available system programs.

This setup is for dedicated usage to build Viking, primarily under Wine with:

 ./generate_install.sh

Such that any Windows system (or of course a Virtual Machine) can be simply deleted (e.g. rm -rf /.wine) and recreated fully via the scripts.

From Viking 1.4 onwards this is how the Windows viking-W.X.Y.Z.exe installer file is produced.
