# How to generate HTML from the xml:
#xmlto xhtml -m config.xsl index.docbook
# ATM config is setup for a single page - index.html

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
pandoc -f docbook georef_layer.xml -t markdown -o help-gr.md
pandoc -f docbook geoclue_layer.xml -t markdown -o help-gc.md
pandoc -f docbook mapnik_rendering_layer.xml -t markdown -o help-mrl.md

# Copy the above outputted files together, e.g.:
cat help-*.md > vikhelp.md

# Fix APPNAME & APP not being repladed in the .md output, e.g.:
sed -i s/APPNAME/viking/g vikhelp.md
sed -i s/APP/Viking/g vikhelp.md

# *Manually* fix broken cross refs:
## Search for ??? in the file and replace with the appropriate string
## (i.e. the text following the next '#' )

# Then dump the entirity of the vikhelp.md into the single Wiki Help page.

# To generate the PDF:
dblatex index.docbook -o viking.pdf
