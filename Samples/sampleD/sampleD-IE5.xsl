<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <HTML>
      <BODY>

        <!-- show the file elements -->
        <xsl:for-each select="files/file">
          <xsl:value-of select="text()"/>
          <br/>
        </xsl:for-each>
      </BODY>
    </HTML>
  </xsl:template>
</xsl:stylesheet>
