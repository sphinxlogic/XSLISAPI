<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <HTML>
      <BODY>
        <table border="1">

          <!-- Here we show the title-->
          <caption>On Backorder</caption>

            <!-- Table header "Fruit" and "Price" -->
            <tr>
              <th>
                Fruit
              </th>
              <th>
                Price
              </th>
            </tr>

            <!-- Here for each of the fruit, display the name and price with dollar sign-->
            <xsl:for-each select="fruits/fruit">
              <tr>
                <td>
                  <xsl:value-of select="name"/>
                </td>
                <td width="20">
                  $<xsl:value-of select="price"/>
                </td>
              </tr>
            </xsl:for-each>

        </table>
      </BODY>
    </HTML>
  </xsl:template>
</xsl:stylesheet>
