<?xml version="1.0" ?> 
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
 <xsl:template match="/">
  <HTML>
   <BODY>
    <xsl:value-of select="test/comment1"/>
    <xsl:value-of select="test/comment2"/>
    </BODY>
  </HTML>
 </xsl:template>
</xsl:stylesheet>