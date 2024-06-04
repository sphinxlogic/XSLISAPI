<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <HTML>
      <BODY>
        <table>
          <tr>
            <td>
              <b>HTML IE5.0</b>
            </td>
          </tr>
          <xsl:for-each select="samples/sample">
            <tr>
              <td>
                <a>
                  <xsl:attribute name="href"><xsl:value-of select="fileName"/></xsl:attribute>
                  <xsl:value-of select="name"/>
                </a>
              </td>
            </tr>
          </xsl:for-each>
        </table>
      </BODY>
    </HTML>
  </xsl:template>
</xsl:stylesheet>
