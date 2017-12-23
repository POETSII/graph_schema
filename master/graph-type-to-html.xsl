<?xml version="1.0"?>
<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:p="http://TODO.org/POETS/virtual-graph-schema-v1"
	id="graph-type-to-html"
    version="1.0"
>

    <xsl:output method="xml" omit-xml-declaration="yes" indent="yes" />

    <xsl:template match="/">
        <html>
            <head>
            </head>
            <body>
                <xsl:call-template name="toc" />
                <xsl:apply-templates select="/p:Graphs/p:GraphType" />
            </body>
        </html>
    </xsl:template>

    <xsl:template name="toc">
        <h1>Table of contents</h1>
        <ul>
            <xsl:for-each select="/p:Graphs/p:GraphType">
                <li>GraphType: <a><xsl:attribute name="href">#<xsl:value-of select="./@id"/></xsl:attribute> <xsl:value-of select="./@id"/></a> </li>    
            </xsl:for-each>
            <xsl:for-each select="/p:Graphs/p:GraphInstance">
                <li>GraphInstance: <a><xsl:attribute name="href">#<xsl:value-of select="./@id"/></xsl:attribute> <xsl:value-of select="./@id"/></a> </li>    
            </xsl:for-each>
        </ul>
    </xsl:template>

    <xsl:template match="/p:Graphs/p:GraphType">
        <h1>GraphType <a><xsl:attribute name="id" ><xsl:value-of select="./@id" /></xsl:attribute><xsl:value-of select="./@id" /></a> </h1>
        <xsl:apply-templates select="p:DeviceTypes/p:DeviceType" />
    </xsl:template>

    <xsl:template match="p:DeviceType">
        <h2>DeviceType <a><xsl:attribute name="id" >#<xsl:value-of select="../../@id" />_<xsl:value-of select="./@id" /></xsl:attribute><xsl:value-of select="./@id" /></a> </h2>

        <table>
            <tr>
                <th>Thing</th>
                <td>Full global C name</td>
                <td>Device-local alias</td>
            </tr>
            <tr>
                <td>Properties</td>
                <td><xsl:call-template name="device_properties_type_name" /></td>
                <td>DEVICE_PROPERTIES_T</td>
            </tr>
            <tr>
                <td>State</td>
                <td><xsl:call-template name="device_state_type_name"><xsl:with-param name="device" select="." /></xsl:call-template></td>
                <td>DEVICE_STATE_T</td>
            </tr>
        </table>

        <xsl:apply-templates select="p:InputPort" />
        <xsl:apply-templates select="p:OutputPort" />
        <xsl:apply-templates select="p:OnCompute" />

    </xsl:template>

    <xsl:template match="p:InputPort">
        <h2>InputPort <a>
            <xsl:attribute name="id" >#<xsl:value-of select="../../../@id" />_<xsl:value-of select="./../@id" />__<xsl:value-of select="./@name" /></xsl:attribute>
            <xsl:value-of select="./@name" />
        </a> </h2>
        <pre>
            <xsl:copy-of select="./p:OnReceive/text()" />
        </pre>
    </xsl:template>

    <xsl:template match="p:OutputPort">
        <h2>InputPort <a>
            <xsl:attribute name="id" >#<xsl:value-of select="../../../@id" />_<xsl:value-of select="./../@id" />__<xsl:value-of select="./@name" /></xsl:attribute>
            <xsl:value-of select="./@name" />
        </a> </h2>

        Handler:
        <pre>
            
        </pre>
        <pre>
            <xsl:copy-of select="./p:OnSend/text()" />
        </pre>
    </xsl:template>

    <xsl:template match="p:OnCompute">
        <h2>OnCompute<a>
            <xsl:attribute name="id" >#<xsl:value-of select="../../../@id" />_<xsl:value-of select="./../@id" />_compute</xsl:attribute>
        </a> </h2>
        Handler:
        <pre>
            <xsl:copy-of select="./text()" />
        </pre>
    </xsl:template>

    <xsl:template name="device_properties_type_name">
        <xsl:param name="device" select="." />
        <xsl:choose>
            <xsl:when test="$device/p:Properties/@cTypeName"><xsl:value-of select="$device/p:Properties/@cTypeName" /></xsl:when>
            <xsl:otherwise><xsl:value-of select="$device/../../@id" />_<xsl:value-of select="$device/@id" />_properties_t</xsl:otherwise>
        </xsl:choose>
    </xsl:template>

    <xsl:template name="device_state_type_name">
        <xsl:param name="device" select="." />
        <xsl:choose>
            <xsl:when test="$device/p:State/@cTypeName"><xsl:value-of select="$device/p:State/@cTypeName" /></xsl:when>
            <xsl:otherwise><xsl:value-of select="$device/../../@id" />_<xsl:value-of select="$device/@id" />_state_t</xsl:otherwise>
        </xsl:choose>
    </xsl:template>

    <!-- http://stackoverflow.com/a/3378562 -->
    <xsl:template match="*">
        <xsl:message terminate="no">
        WARNING: Unmatched element: <xsl:value-of select="name()"/>
        </xsl:message>

        <xsl:apply-templates/>
    </xsl:template>

</xsl:stylesheet>