<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <HDML VERSION="3.0" PUBLIC="TRUE" MARKABLE="TRUE" TTL="0">
      <DISPLAY>

        <!-- show the file elements-->
        <xsl:for-each select="files/file">
          <xsl:value-of select="text()"/>, 
        </xsl:for-each>
      </DISPLAY>
    </HDML>
  </xsl:template>
</xsl:stylesheet>
