<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <HDML VERSION="3.0" PUBLIC="TRUE" MARKABLE="TRUE" TTL="0">
      <DISPLAY>
        Fruit Salad Ingredients

        <!-- Display the name of each fruit-->
        <xsl:for-each select="/fruit_salad_ingredients/fruit">
          <xsl:value-of select="name"/>
          <BR/>
        </xsl:for-each>
      </DISPLAY>
    </HDML>
  </xsl:template>
</xsl:stylesheet>
