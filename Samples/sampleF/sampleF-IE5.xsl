<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <HTML>
      <BODY>

        <!-- Show the document content -->
        <xsl:for-each select="//line">
          <br/>
          <xsl:value-of select="text()"/> 
        </xsl:for-each>

      </BODY>
    </HTML>
  </xsl:template>
</xsl:stylesheet>
