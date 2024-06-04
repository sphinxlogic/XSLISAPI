<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <HDML VERSION="3.0" PUBLIC="TRUE" MARKABLE="TRUE" TTL="0">
      <DISPLAY>

        <!-- Show the name of each color-->
        <xsl:for-each select="colors/color">
          <xsl:value-of select="name"/>, 
        </xsl:for-each>
      </DISPLAY>
    </HDML>
  </xsl:template>
</xsl:stylesheet>
