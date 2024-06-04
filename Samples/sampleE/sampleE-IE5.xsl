<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <HTML>
      <BODY>
        <table border="1">

          <!-- for each color invoke the following -->
          <xsl:for-each select="colors/color">
            <tr>
              <td>
                <xsl:value-of select="traditional_Chinese_character"/>
              </td>
              <td width="20">

                <!-- display the traditional chinese character and use the corresponding color as the cell color-->  
                <xsl:attribute name="bgcolor"><xsl:value-of select="value"/></xsl:attribute>
              </td>
            </tr>
          </xsl:for-each>
        </table>
      </BODY>
    </HTML>
  </xsl:template>
</xsl:stylesheet>
