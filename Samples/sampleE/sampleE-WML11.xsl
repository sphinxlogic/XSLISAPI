<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <xsl:pi name="xml">version="1.0"</xsl:pi>
    <xsl:doctype>wml PUBLIC "-//WAPFORUM//DTD WML 1.1//EN" "http://www.wapforum.org/DTD/wml_1.1.xml"</xsl:doctype>
    <wml>
      <card>
        <p>

          <!-- for each color invoke the following -->
          <xsl:for-each select="colors/color">

            <!-- display the traditional chinese character for the corresponding color-->
            <xsl:value-of select="traditional_Chinese_character"/>
            <br/>
          </xsl:for-each>
        </p>
      </card>
    </wml>
  </xsl:template>
</xsl:stylesheet>
