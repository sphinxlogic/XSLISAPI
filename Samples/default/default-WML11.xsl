<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <xsl:pi name="xml">version="1.0"</xsl:pi>
    <xsl:doctype>wml PUBLIC "-//WAPFORUM//DTD WML 1.1//EN" "http://www.wapforum.org/DTD/wml_1.1.xml"</xsl:doctype>
    <wml>
      <card>
        <p mode="nowrap">
          WML 1.1
          <xsl:for-each select="samples/sample">
            <br/>
            <a><xsl:attribute name="href"><xsl:value-of select="fileName"/></xsl:attribute><xsl:value-of select="name"/></a>
          </xsl:for-each>
        </p>
      </card>
    </wml>
  </xsl:template>
</xsl:stylesheet>
