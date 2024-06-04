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

    This particular one is for basic HTML browsers.
-->

<HTML xmlns:xsl="http://www.w3.org/TR/WD-xsl">

<h3>XSL ISAPI Error</h3>

<i>Description:</i> <xsl:value-of select="/error/info"/><br/>
<i>URL:</i> <xsl:value-of select="/error/url"/><br/>
<i>Status Code:</i> <xsl:value-of select="/error/status-code"/><br/>

</HTML>
