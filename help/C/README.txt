# How to generate HTML from the xml:
xmlto xhtml -m config.xsl viking.xml
# ATM config is setup for a single page - index.html

# To generate a block of text for the Wiki:
html2wiki --dialect=MediaWiki \
--wiki-uri=http://sourceforge.net/apps/mediawiki/viking \
--base-uri=http://sourceforge.net/apps/mediawiki/viking \
index.html > help.wiki
