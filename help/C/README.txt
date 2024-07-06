# How to generate HTML from the xml:
xmlto html -m config.xsl index.docbook
# ATM config is setup for a single page - index.html
# then copy the generated *.html files and all files from figures/ into the version to upload
# (e.g. into viking-gps.github.io)

# To generate a block of text for the Wiki:
#html2wiki --dialect=MediaWiki \
#--wiki-uri=http://sourceforge.net/p/viking/wikiallura \
#--base-uri=http://sourceforge.net/p/viking/wikiallura \
#index.html > help.wiki

# Allura Wiki is in Markdown
# pandoc -f docbook index.docbook -t markdown -o help.md
# Note pandoc (at least 1.17.5.4) doesn't open included files within docbook,
#  if using xmlto (but output not that great):
#pandoc -f html index.html -t markdown -o help.md

# Otherwise manually resolve pandoc issues:
# Run for each instance:
pandoc -f docbook index.docbook -t markdown -o help-all.md
for x in $(ls [edgm]*.xml); do pandoc -f docbook $x -o help-$x.md; done
# These done in specific order to help the joining order
pandoc -f docbook recommends.xml -t markdown -o help-r.md
pandoc -f docbook commandline.xml -t markdown -o help-u.md
pandoc -f docbook attribution.xml -t markdown -o help-w.md
pandoc -f docbook legal.xml -t markdown -o help-z.md

# Copy the above outputted files together (with a blank line between them) e.g.:
for x in $(ls help-*.md); do cat $x >> vikhelp.md; echo >> vikhelp.md; done

# Fix APPNAME, APP & DHPACKAGE not being replaced in the .md output, e.g.:
sed -i s/APPNAME/viking/g vikhelp.md
sed -i s/APP/Viking/g vikhelp.md
sed -i s/DHPACKAGE//g vikhelp.md

#
# e.g. use 'retext' program (https://github.com/retext-project/retext) viewer/editor
#

# *Manually* fix broken cross refs:
## Search for ??? in the file and replace with the appropriate string
## (i.e. the text following the next '#' )

# Manually generate an initial contents - don't know an automatic way
## view output with yelp (yelp index.docbook)
## Copy and paste contents list into vikhelp.md
## for each item edit + add the appropriate [Name]'{#linkname}' to generate an internal link
### Each section should now have an id for the linkname in the source help
## (or maybe reuse a previous version of these contents as follows:)

[Introduction](#Introduction)

[General Concepts](#GeneralConcepts)

[File Types and the Main Window](#FileTypes)

[Edit and View Menus](#EditViewMenu)

[Layers](#Layers)

[TrackWaypoint Layer](#TrackWaypoint)

[GPS Layer](#GPS)

[DEM (Digital Elevation Model) Layer](#DEM)

[Map Layer](#Map)

[Aggregate Layer](#Aggregate)

[GeoRef Layer](#GeoRef)

[GeoClue Layer](#GeoClue)

[Mapnik Rendering Layer](#MapnikRendering)

[Coordinate Layer](#Coordinate)

[Tools](#tools)

[Preferences](#prefs)

[Howto's](#Howto)

[Extending Viking](#extend_viking)

[Recommended Programs](#recommends)

[Command Line](#commandline)

[Attributions](#attrib)

[Legal](#Legal)

# Then dump the entirity of the vikhelp.md into the single Wiki Help page.

# To generate the PDF:
dblatex -p style.xsl index.docbook -o viking.pdf
