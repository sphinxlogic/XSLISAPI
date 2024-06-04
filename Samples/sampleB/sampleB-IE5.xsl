<?xml version="1.0" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <HTML>
      <BODY>
        <table border="1">
          <th>
            Fruit Salad Ingredients
          </th>

          <!-- Display the name of each fruit-->
          <xsl:for-each select="/fruit_salad_ingredients/fruit">
            <tr>
              <td>
                <xsl:value-of select="name"/>
              </td>
            </tr>
          </xsl:for-each>
        </table>
      </BODY>
    </HTML>
  </xsl:template>
</xsl:stylesheet>
