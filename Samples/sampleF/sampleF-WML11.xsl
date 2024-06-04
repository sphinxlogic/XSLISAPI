<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <xsl:pi name="xml">version="1.0"</xsl:pi>
    <xsl:doctype>wml PUBLIC "-//WAPFORUM//DTD WML 1.1//EN" "http://www.wapforum.org/DTD/wml_1.1.xml"</xsl:doctype>
    <wml>
      <card>
        <p>
          Current page:
          <xsl:value-of select="//pageNumber"/>
          <br/>

          <!-- Show only the desired page -->
          <xsl:apply-templates select="//page[ index() $eq$ //pageNumber ]"/>

          <!-- show the link to the previous page -->
          <xsl:for-each select="//prev">
            <br/>
            <a><xsl:attribute name="href">sampleF.pasp?page=<xsl:value-of select="//prev"/>&amp;size=<xsl:value-of select="//size"/></xsl:attribute>Prev page: <xsl:value-of select="//prev"/></a>
          </xsl:for-each>

          <!-- show the link to the next page -->
          <xsl:for-each select="//next">
            <br/>
            <a><xsl:attribute name="href">sampleF.pasp?page=<xsl:value-of select="//next"/>&amp;size=<xsl:value-of select="//size"/></xsl:attribute>Next page: <xsl:value-of select="//next"/></a>
          </xsl:for-each>
        </p>
      </card>
    </wml>
  </xsl:template>

  <xsl:template match="page">
        <xsl:for-each select="./line">
          <xsl:value-of select="text()"/>
         <br/>
        </xsl:for-each>
  </xsl:template>

</xsl:stylesheet>
