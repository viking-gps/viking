# How to generate HTML from the xml:
xmlto xhtml -m config.xsl viking.xml
# ATM config is setup for a single page - index.html

# To generate a block of text for the Wiki:
html2wiki --dialect=MediaWiki \
--wiki-uri=http://sourceforge.net/p/viking/wikiallura \
--base-uri=http://sourceforge.net/p/viking/wikiallura \
index.html > help.wiki

# To generate the PDF:
dblatex viking.xml
