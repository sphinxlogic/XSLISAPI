<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <HDML VERSION="3.0" PUBLIC="TRUE" MARKABLE="TRUE" TTL="0">
      <DISPLAY>

        <!-- Here we show the title-->
        On Backorder

        <!-- For each fruit element-->
        <xsl:for-each select="fruits/fruit">
          <BR/>

          <!-- Show the name and price witht the $ sign-->
          <xsl:value-of select="name"/>
          $<xsl:value-of select="price"/>
        </xsl:for-each>
      </DISPLAY>
    </HDML>
  </xsl:template>
</xsl:stylesheet>
