<?xml version="1.0" encoding="utf-8"?>
<!--
 * viking - GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2010, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
-->
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:h="http://www.w3.org/1999/xhtml">

<xsl:output method="text"/>

  <xsl:template match="/">
<xsl:text>/* Generated file. */
const gchar *DOCUMENTERS[] = {\
</xsl:text>
    <xsl:for-each select="//author">
      <xsl:text>"</xsl:text>
      <xsl:value-of select="firstname"/>
      <xsl:text> </xsl:text>
      <xsl:value-of select="surname"/>
      <xsl:text>",\
</xsl:text>
    </xsl:for-each>
      <xsl:text>NULL};
</xsl:text>
  </xsl:template>

</xsl:stylesheet>
