# How to generate HTML from the xml:
xmlto xhtml -m config.xsl viking.xml
# ATM config is setup for a single page - index.html

# To generate a block of text for the Wiki:
#html2wiki --dialect=MediaWiki \
#--wiki-uri=http://sourceforge.net/p/viking/wikiallura \
#--base-uri=http://sourceforge.net/p/viking/wikiallura \
#index.html > help.wiki

# Allura Wiki is in Markdown
# pandoc -f docbook viking.xml -t markdown -o help.md
# Note pandoc (at least 1.12.4.2) doesn't open included files within docbook,
#  so stick with using xmlto for now
pandoc -f html index.html -t markdown -o help.md

# To generate the PDF:
dblatex viking.xml
