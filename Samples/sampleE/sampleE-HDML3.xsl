<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <HDML VERSION="3.0" PUBLIC="TRUE" MARKABLE="TRUE" TTL="0">
      <DISPLAY>

        <!-- for each color invoke the following -->
        <xsl:for-each select="colors/color">

          <!-- display the traditional chinese character for the corresponding color-->
          <xsl:value-of select="traditional_Chinese_character"/>, 
        </xsl:for-each>
      </DISPLAY>
    </HDML>
  </xsl:template>
</xsl:stylesheet>
