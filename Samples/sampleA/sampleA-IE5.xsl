<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <HTML>
      <BODY>
        <table border="1">
          <xsl:for-each select="colors/color">
            <tr>
              <td>

                <!-- Show the name of each color-->
                <xsl:value-of select="name"/>
              </td>
              <td width="20">

                <!-- Set the corresponding background color-->
                <xsl:attribute name="bgcolor"><xsl:value-of select="value"/></xsl:attribute>
              </td>
            </tr>
          </xsl:for-each>
        </table>
      </BODY>
    </HTML>
  </xsl:template>
</xsl:stylesheet>
