<?xml version='1.0'?>
<xsl:stylesheet version="1.0" 
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:r="http://relaxng.org/ns/structure/1.0" 
    xmlns:a="http://relaxng.org/ns/compatibility/annotations/1.0"
>
<xsl:output method="text" />

<xsl:template match="/">
    <!--    <xsl:apply-templates select="//r:define[a:documentation] | //r:attribute[a:documentation]" /> -->
    <xsl:apply-templates select="r:grammar" />
</xsl:template>

<xsl:template match="r:grammar">
    <xsl:apply-templates select="r:start" />
</xsl:template>

<xsl:template match="r:start">
    Start
</xsl:template>

<xsl:template match="r:define">
    Wibble
    <xsl:variable name="doc" select="a:documentation" />
    <xsl:call-template name="print-path">
        <xsl:with-param name="elm" select="//r:element[r:ref/@name=current()/@name]" />
    </xsl:call-template>
    <xsl:value-of select="$doc" /><xsl:text>&#10;</xsl:text>
</xsl:template>

<xsl:template match="r:attribute">
    <xsl:variable name="doc" select="a:documentation" />
    <xsl:call-template name="print-path">
        <xsl:with-param name="elm" select="//r:element[r:ref/@name=current()/ancestor::r:define/@name]" />
        <xsl:with-param name="path" select="concat('/@',@name)" />
    </xsl:call-template>
    <xsl:value-of select="$doc" /><xsl:text>&#10;</xsl:text>
</xsl:template>

<xsl:template name="print-path">
    <xsl:param name="elm" />
    <xsl:param name="path" />

    <xsl:variable name="parent" select="//r:ref[@name=$elm/ancestor::r:define/@name]/ancestor::r:element" />
    <xsl:message><xsl:value-of select="$elm/@name" /></xsl:message>
    <xsl:choose>
        <xsl:when test="$parent">
            <xsl:call-template name="print-path">
                <xsl:with-param name="elm" select="$parent" />
                <xsl:with-param name="path" select="concat('/',$elm/@name,$path)" />
            </xsl:call-template>
        </xsl:when>
        <xsl:otherwise>
            <xsl:value-of select="concat('/',$elm/@name,$path)" /><xsl:text>&#10;</xsl:text>            
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>

</xsl:stylesheet>
