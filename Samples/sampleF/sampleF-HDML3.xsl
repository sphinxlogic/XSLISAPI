<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <HDML VERSION="3.0" PUBLIC="TRUE" MARKABLE="TRUE" TTL="0">
      <DISPLAY>
          Current page:
          <xsl:value-of select="//pageNumber"/>

          <!-- Show only the desired page -->
          <xsl:apply-templates select="//page[ index() $eq$ //pageNumber ]"/>

          <!-- show the link to the previous page -->
          <xsl:for-each select="//prev">
            <a task="go"><xsl:attribute name="dest">sampleF.pasp?page=<xsl:value-of select="//prev"/>&amp;size=<xsl:value-of select="//size"/></xsl:attribute>Prev page: <xsl:value-of select="//prev"/></a>
          </xsl:for-each>

          <!-- show the link to the next page -->
          <xsl:for-each select="//next">
              <a task="go"><xsl:attribute name="dest">sampleF.pasp?page=<xsl:value-of select="//next"/>&amp;size=<xsl:value-of select="//size"/></xsl:attribute>Next page: <xsl:value-of select="//next"/></a>
          </xsl:for-each>
      </DISPLAY>
    </HDML>
  </xsl:template>

  <xsl:template match="page">
        <xsl:for-each select="./line">
          <BR/><xsl:value-of select="text()"/>
        </xsl:for-each>
  </xsl:template>

</xsl:stylesheet>