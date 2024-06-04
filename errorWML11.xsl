<?xml version="1.0" ?>

<!--
    This stylesheet is designed for processing a very special
    purpose XML input document containing server HTTP error
    information.  For exampe:
        <error>
                <status-code>404</status-code>
                <url>foobar.xml</url>
                <info>Resource not found</info>
        </error>

    This then allows you to localize, or customize this error 
    page how ever you want.

    This particular one is for WML 1.1 browsers.
-->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/TR/WD-xsl">
  <xsl:template match="/" >
    <xsl:pi name="xml">version="1.0"</xsl:pi>
    <xsl:doctype>wml PUBLIC "-//WAPFORUM//DTD WML 1.1//EN" "http://www.wapforum.org/DTD/wml_1.1.xml"</xsl:doctype>
    <wml>
      <card>
      <p mode="nowrap">
      XSL ISAPI Error<br/>
      <xsl:value-of select="error/info"/><br/>
      URL   : <xsl:value-of select="error/url"/><br/>
      Status: <xsl:value-of select="error/status-code"/><br/>
      </p>
      </card>
    </wml>
  </xsl:template>
</xsl:stylesheet>
