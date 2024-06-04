<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <HDML VERSION="3.0" PUBLIC="TRUE" MARKABLE="TRUE" TTL="0">
      <CHOICE>
        <LINE>HDML 3.0</LINE>
        <xsl:for-each select="samples/sample">
          <CE TASK="GO" LABEL="GO">
            <xsl:attribute name="DEST"><xsl:value-of select="fileName"/></xsl:attribute>
          </CE>
          <xsl:value-of select="name"/>
        </xsl:for-each>
      </CHOICE>
    </HDML>
  </xsl:template>
</xsl:stylesheet>
