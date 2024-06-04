<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">

  <!-- Identity transformation template -->
  <xsl:template><xsl:copy><xsl:apply-templates select="@* | * | comment() | pi() | text()"/></xsl:copy></xsl:template>

  <!-- Filter out Oranges -->
  <xsl:template match="fruit[name = 'oranges']"/>

</xsl:stylesheet>

