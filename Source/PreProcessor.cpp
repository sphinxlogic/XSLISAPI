// ============================================================================
// FILE: PreProcessor.cpp
//
//        Implementation of the actual scanner/preprocessor that
//        converts from .pasp files to .asp files.  (This is used by
//        the ASPPreprocessor COM object.)
//
// Copyright (c) 1999-2000 Microsoft Corporation. All rights reserved.

#include "StdAfx.h"
#include "PreProcessor.h"

// Preprocessor input state. Indicates where in the input file the
// scanner is.

enum PPINPUTSTATE
{
    PPF_IN_TEXT,                            // In outside text
    PPF_IN_SERVER_SCRIPT,                   // Inside a <SCRIPT RUNAT=server>
    PPF_IN_SERVER_OBJECT,                   // Inside an <OBJECT RUNAT=server>
    PPF_IN_PROCESSING_DIRECTIVE,            // Inside <% %>
    PPF_IN_AT_DIRECTIVE,                    // Inside <%@ %>
    PPF_IN_OUTPUT_DIRECTIVE,                // Inside <%= %>
};

// ============================================================================
// STRUCT: LANGDATA
//         Language specific data. There are two of these, one of VBScript and
//         one for JScript. The preprocessor figures out the source language,
//         and uses the appropriate on. 

struct LANGDATA
{
    bool bIsVBScript;                       // true=VBScript, false=JScript
    LPCSTR pszDocGenerate;                  // String to generate XML from AML
    int nDocGenerateLength;                 // Length of string above
    LPCSTR pszWritePrefix;                  // Prefix before a Write call
    int nWritePrefixLength;                 // Length of string above
    LPCSTR pszWriteLinePrefix;              // Prefix before a WriteLine call
    int nWriteLinePrefixLength;             // Length of string above
    LPCSTR pszWriteSuffix;                  // Suffix after a Write or WriteLine call
    int nWriteSuffixLength;                 // Length of string above.
};


// ============================================================================
// STRUCT: SCANSTATE
//         Preprocessor state, passed between functions.

struct SCANSTATE
{
    Writer *pWriter;                        // supplied Writer interface
    LPCSTR pszPos;                          // Current position in source buffer
    LANGDATA lang;                          // Language data
    PPINPUTSTATE dwInputState;              // Current input state (PPF_IN_*)
    bool bInTag;                            // Currently inside a tag in source
    int nQuoteState;                        // Current quote state
    LPCSTR pszProcessingHint;               // Hint for processing next block
};


// String constants for things in source file. Must be all uppercase, so
// that they can work with StringMatch functions.

DECLARE_GLOBALSTRING (LanguageDirective, "LANGUAGE");
DECLARE_GLOBALSTRING (ObjectName, "OBJECT");
DECLARE_GLOBALSTRING (ScriptName, "SCRIPT");
DECLARE_GLOBALSTRING (RunAt, "RUNAT");
DECLARE_GLOBALSTRING (Server, "SERVER");
DECLARE_GLOBALSTRING (VBScript, "VBSCRIPT");
DECLARE_GLOBALSTRING (JScript, "JSCRIPT");
DECLARE_GLOBALSTRING (ResponseWrite, "RESPONSE.WRITE");
DECLARE_GLOBALSTRING (ResponseFlush, "RESPONSE.FLUSH");
DECLARE_GLOBALSTRING (ResponseClear, "RESPONSE.CLEAR");
DECLARE_GLOBALSTRING (ResponseEnd, "RESPONSE.END");

// Strings to write out. For some strings, there are different versions for
// VBScript and JScript.

DECLARE_GLOBALSTRING (XMLServDocDecl, 
    "<OBJECT RUNAT=server ID=XMLServDoc PROGID=\"XSLISAPI.XMLServerDocument\"></OBJECT>\r\n");

DECLARE_GLOBALSTRING (XMLServDocGenerate_VBScript,
     "XMLServDoc.URL = Request.ServerVariables(\"HTTP_SSXSLSRCFILE\")\r\n"
     "XMLServDoc.UserAgent = Request.ServerVariables(\"HTTP_USER_AGENT\")\r\n"
     "On Error Resume Next\r\n"
     "XMLServDoc.Transform Response\r\n"
     "If Err.Number <> 0 Then\r\n"
     "    XMLServDoc.HandleError Response\r\n"
     "End If\r\n"
    );

DECLARE_GLOBALSTRING (XMLServDocGenerate_JScript,
     "XMLServDoc.URL = Request.ServerVariables(\"HTTP_SRCFILE\");\r\n"
     "XMLServDoc.UserAgent = Request.ServerVariables(\"HTTP_USER_AGENT\");\r\n"
     "try {\r\n"                      
     "    XMLServDoc.Transform(Response);\r\n"
     "} catch (exception) {\r\n"                      
     "    XMLServDoc.HandleError(Response);\r\n"                      
     "}\r\n"                  
    );

DECLARE_GLOBALSTRING (XMLServDocWritePrefix_VBScript, "XMLServDoc.Write ");
DECLARE_GLOBALSTRING (XMLServDocWritePrefix_JScript, "XMLServDoc.Write(");
DECLARE_GLOBALSTRING (XMLServDocWriteGeneric, "XMLServDoc.Write");
DECLARE_GLOBALSTRING (XMLServDocFlushGeneric, "XMLServDoc.Flush");
DECLARE_GLOBALSTRING (XMLServDocClearGeneric, "XMLServDoc.Clear");
DECLARE_GLOBALSTRING (XMLServDocEndGeneric, "XMLServDoc.End");
DECLARE_GLOBALSTRING (XMLServDocWriteLinePrefix_VBScript, "XMLServDoc.WriteLine ");
DECLARE_GLOBALSTRING (XMLServDocWriteLinePrefix_JScript, "XMLServDoc.WriteLine(");
DECLARE_GLOBALSTRING (XMLServDocWriteSuffix_VBScript, "\r\n");
DECLARE_GLOBALSTRING (XMLServDocWriteSuffix_JScript, ");\r\n");

// Language data objects for VBScript and JScript.

static const LANGDATA LangData[] = {
    
    { 
        true,   // VBScript
        GLOBALSTRING(XMLServDocGenerate_VBScript), GLOBALSTRING_LEN(XMLServDocGenerate_VBScript),
        GLOBALSTRING(XMLServDocWritePrefix_VBScript), GLOBALSTRING_LEN(XMLServDocWritePrefix_VBScript),
        GLOBALSTRING(XMLServDocWriteLinePrefix_VBScript), GLOBALSTRING_LEN(XMLServDocWriteLinePrefix_VBScript),
        GLOBALSTRING(XMLServDocWriteSuffix_VBScript), GLOBALSTRING_LEN(XMLServDocWriteSuffix_VBScript),
    },

    { 
        false,  // JScript
        GLOBALSTRING(XMLServDocGenerate_JScript), GLOBALSTRING_LEN(XMLServDocGenerate_JScript),
        GLOBALSTRING(XMLServDocWritePrefix_JScript), GLOBALSTRING_LEN(XMLServDocWritePrefix_JScript),
        GLOBALSTRING(XMLServDocWriteLinePrefix_JScript), GLOBALSTRING_LEN(XMLServDocWriteLinePrefix_JScript),
        GLOBALSTRING(XMLServDocWriteSuffix_JScript), GLOBALSTRING_LEN(XMLServDocWriteSuffix_JScript),
    },
};

// Indexes into language data.

enum LanguageType
{
    LANG_VBSCRIPT = 0,
    LANG_JSCRIPT = 1,
};


// Forward function declarations

HRESULT FindLanguageDirective(SCANSTATE* pss);
bool CheckCodeBlock(LPCSTR pszScan, LPCSTR* ppszCodeBlockStart);
HRESULT ConvertTextBlock(SCANSTATE* pss);
HRESULT ConvertDirective(SCANSTATE* pss);
HRESULT ConvertScriptCodeBlock(SCANSTATE* pss);
HRESULT ConvertObjectCodeBlock(SCANSTATE* pss);


// ============================================================================
// ScanAndPreprocess 
//      Main function to preprocess an ASP file. Goes through the file
//      block by block, processing each block. 
//      Input must be provided through a NUL-terminated buffer of ASCII
//      characters (UTF8 is OK). 
//      Output is done through an Out() method on the supplied
//      Writer object.
//      Returns HRESULT indicating success or failure.

HRESULT
ScanAndPreprocess(
    LPCSTR pszBuffer,                       // Buffer pointing to ASP (NUL-terminated)
    DWORD dwBufferLength,                   // Length of buffer (excluding NUL)
    Writer & writer)                        // client supplied writing functionality
{
    // Figure out buffer length if not passed (this is not recommended).
    if (dwBufferLength == static_cast<DWORD>(-1L))
    {
        dwBufferLength = lstrlenA (pszBuffer);
    }

    // Short circuit on empty buffer.
    if (dwBufferLength == 0)
    {
        return S_OK;
    }

    ASSERT(pszBuffer != NULL);
    ASSERT(!IsBadReadPtr (pszBuffer, dwBufferLength + 1));
    ASSERT(pszBuffer[dwBufferLength] == '\0');

    // Initialize preprocessor state.

    SCANSTATE ss;
    ss.pWriter      = &writer;
    ss.pszPos       = pszBuffer;
    ss.dwInputState = PPF_IN_TEXT;
    ss.lang         = LangData[0];
    ss.nQuoteState  = 0;
    ss.bInTag       = false;

    HRESULT hr;

    // Check for an @LANGUAGE directive in the file. This must appear in the
    // first 1K of the file.
    if (FAILED(hr = FindLanguageDirective (&ss)))
    {
        return hr;
    }

    // Write out the prologue.
    if (FAILED(hr = writer.Out (GLOBALSTRING(XMLServDocDecl),
                                GLOBALSTRING_LEN(XMLServDocDecl))))
    {
        return hr;
    }
    if (FAILED(hr = writer.Out ("<%\r\n", 4)))
    {
        return hr;
    }

    // Go through each block.

    while (*ss.pszPos != '\0')
    {
        switch (ss.dwInputState)
        {
            case PPF_IN_TEXT:
                hr = ConvertTextBlock (&ss);
                break;
            case PPF_IN_PROCESSING_DIRECTIVE:
            case PPF_IN_OUTPUT_DIRECTIVE:
            case PPF_IN_AT_DIRECTIVE:
                hr = ConvertDirective (&ss);
                break;
            case PPF_IN_SERVER_OBJECT:
                hr = ConvertObjectCodeBlock (&ss);
                break;
            case PPF_IN_SERVER_SCRIPT:
                hr = ConvertScriptCodeBlock (&ss);
                break;
        }

        if (FAILED(hr))
            return hr;
    }

    // Write out the epilogue.
    if (FAILED(hr = writer.Out (ss.lang.pszDocGenerate,
                                ss.lang.nDocGenerateLength)))
    {
        return hr;
    }
    if (FAILED(hr = writer.Out ("%>", 4)))
    {
        return hr;
    }
    return S_OK;
}

// ============================================================================
// FindLanguageDirective
//      Finds the @LANGUAGE directive in the file. Doesn't bother checking 
//      for whether the directive is in a valid tag. Checking is only done
//      for the first 1K of the file - you can speed up this process by putting
//      a directive at the top of every file.
//      Returns HRESULT indicating success or failure.

HRESULT
FindLanguageDirective(
    SCANSTATE* pss)                         // Preprocessor state
{
    LPCSTR pszScan = pss->pszPos;
    for (int iChar = 0; iChar < 1024 && *pszScan != '\0'; iChar++, pszScan++)
    {
        // Look for sequences starting with '@L'.

        if (*pszScan == '@')
        {
            for (pszScan = pszScan + 1; 
                 CHAR_IS_WHITESPACE (*pszScan) && iChar < 1024; 
                 iChar++, pszScan++)
            {
            }
            
            if (CHAR_TO_UPPER (*pszScan) != GLOBALSTRING(LanguageDirective)[0])
            {
                pszScan--;
                continue;
            }

            // Try to find the LANGUAGE attribute.

            LPCSTR pszAttrValue;
            int nValueLen;
            pszAttrValue = FindAttributeInTag (pszScan, 
                GLOBALSTRING(LanguageDirective), GLOBALSTRING_LEN(LanguageDirective), 
                &nValueLen, '%', &pszScan);

            if (pszAttrValue != NULL)
            {
                if (nValueLen == GLOBALSTRING_LEN(VBScript) && 
                        CHAR_TO_UPPER(*pszAttrValue) == GLOBALSTRING(VBScript)[0] &&
                        StringMatch (pszAttrValue, 
                            GLOBALSTRING(VBScript), GLOBALSTRING_LEN(VBScript)))
                {
                    pss->lang = LangData[0];
                }
                else if (nValueLen == GLOBALSTRING_LEN(JScript) && 
                            CHAR_TO_UPPER(*pszAttrValue) == GLOBALSTRING(JScript)[0] &&
                            StringMatch (pszAttrValue, 
                                GLOBALSTRING(JScript), GLOBALSTRING_LEN(JScript)))
                {
                    pss->lang = LangData[1];
                }
                else
                {
                    // Error - unsupported script type;
                    return E_FAIL;
                }
                break;
            }
        }
    }

    return S_OK;
}

// ============================================================================
// CheckCodeBlock
//      Checks a code block, starting with a <SCRIPT> or <OBJECT> tag, to
//      see whether the tag has the RUNAT attribute set to "server". 
//      Returns true if RUNAT=server, false otherwise.

bool
CheckCodeBlock(
    LPCSTR pszScan,                         // Pointer to start of code
    LPCSTR* ppszCodeBlockStart)             // Receives start of code block.
{
    ASSERT(pszScan != NULL);
    ASSERT(ppszCodeBlockStart != NULL);

    LPCSTR pszAttrValue;
    int nValueLen;
    
    pszAttrValue = FindAttributeInTag (pszScan, GLOBALSTRING(RunAt),
        GLOBALSTRING_LEN(RunAt), &nValueLen, '>', ppszCodeBlockStart);

    if (pszAttrValue != NULL && 
            nValueLen == GLOBALSTRING_LEN(Server) &&
            StringMatch (pszAttrValue, GLOBALSTRING(Server), GLOBALSTRING_LEN(Server)))
    {
        // Advance past > 
        (*ppszCodeBlockStart)++;
        return true;
    }
    else
    {
        return false;
    }
}

// ============================================================================
// ConvertTextBlock
//      Converts a block of text, using multiple Write and WriteLine commands.
//
//      - Each full line xxxxxx becomes XMLServDoc.WriteLine "xxxxxx".
//      - A partial line xxxxxx becomes XMLServDoc.Write "xxxxxx".
//      - Conversion continues until one of the following is found:
//          - end of file
//          - a server SCRIPT or OBJECT tag
//          - an ASP directive
//      Returns HRESULT indicating success or failure.

HRESULT
ConvertTextBlock(
    SCANSTATE* pss)                         // Preprocessor state
{
    LPCSTR pszBegin;
    LPCSTR pszScan = pss->pszPos;

    while (*pszScan != '\0')
    {
        // Go one line at a time.

        bool bAnyDoubleQuotes = false;
        pszBegin = pszScan;
        do
        {
            if (*pszScan == '\x0d')
            {
                // End of line.
                break;
            }
            else if (*pszScan == '\'' && pss->bInTag)
            {
                // We're in a tag, so update quote state.
                pss->nQuoteState = QUOTE_SINGLE_QUOTE_TRANSITION (pss->nQuoteState);
            }
            else if (*pszScan == '\"')
            {
                // If we're in a tag, update quote state.
                if (pss->bInTag)
                {
                    pss->nQuoteState = QUOTE_DOUBLE_QUOTE_TRANSITION (pss->nQuoteState);
                }
                
                // Lines with double quotes require special processing.
                bAnyDoubleQuotes = true;
            }
            else if (*pszScan == '<')
            {
                if (pszScan[1] == '%')
                {
                    // ASP directive. Figure out which one.

                    for (LPCSTR psz = pszScan + 2; CHAR_IS_WHITESPACE (*psz); psz++)
                    {
                    }
                    if (pszScan[2] == '=')
                    {
                        pss->dwInputState = PPF_IN_OUTPUT_DIRECTIVE;
                    }
                    else if (*psz == '@')
                    {
                        pss->dwInputState = PPF_IN_AT_DIRECTIVE;
                    }
                    else
                    {
                        pss->dwInputState = PPF_IN_PROCESSING_DIRECTIVE;
                    }
                    break;
                }
                else if (pss->bInTag)
                {
                    // BUGBUG - Error (tag inside tag?)
                }
                else if (pss->nQuoteState == 0)
                {
                    // Check for <OBJECT> or <SCRIPT> tag. These, when possessing
                    // the RUNAT=server attribute, must be copied to the script,
                    // not the output.

                    char c = CHAR_TO_UPPER (pszScan[1]);
                    if (c == GLOBALSTRING(ObjectName)[0])
                    {
                        if (StringMatchWord (pszScan + 1, GLOBALSTRING(ObjectName),
                                    GLOBALSTRING_LEN(ObjectName)) &&
                                CheckCodeBlock (pszScan + 1, &pss->pszProcessingHint))
                        {
                            pss->dwInputState = PPF_IN_SERVER_OBJECT;
                            break;
                        }
                    } 
                    else if (c == GLOBALSTRING(ScriptName)[0])
                    {
                        if (StringMatchWord (pszScan + 1, GLOBALSTRING(ScriptName),
                                    GLOBALSTRING_LEN(ScriptName)) &&
                                CheckCodeBlock (pszScan + 1, &pss->pszProcessingHint))
                        {
                            pss->dwInputState = PPF_IN_SERVER_SCRIPT;
                            break;
                        }
                    }

                    // Otherwise, we're in a tag.
                    pss->bInTag = true;
                }
            }
            else if (*pszScan == '>' && pss->bInTag && pss->nQuoteState == 0)
            {
                // We are leaving the tag.
                pss->bInTag = false;
            }

        } while (*(++pszScan) != '\0');

        // If there are no characters to output, skip the stage.

        if (pszScan == pszBegin && *pszScan != '\x0d')
        {
            break;
        }

        // Write out the line, beginning with the appropriate Write or 
        // WriteLine command.

        HRESULT hr;
        if (*pszScan == '\x0d')
        {
            hr = pss->pWriter->Out (pss->lang.pszWriteLinePrefix, 
                                    pss->lang.nWriteLinePrefixLength); 
        }
        else
        {
            hr = pss->pWriter->Out (pss->lang.pszWritePrefix, 
                                    pss->lang.nWritePrefixLength);
        }
        if (FAILED(hr))
        {
            return hr;
        }

        if (bAnyDoubleQuotes)
        {
            // If the line contains double quotes, the handling is different.
            // Double quotes in the text are handled using \" escapes in 
            // JScript mode, and "& Chr(34) &" in VBScript mode.
            // For example, 
            //      This is "my" car 
            // is written out as 
            //      "This is \"my\" car" (in JScript)
            // and  "This is " & Chr(34) & "my" & Chr(34) & " car" (in VBScript)

            LPCSTR pszLine = pszBegin;
            bool bIsVBScript = pss->lang.bIsVBScript;
            if (!bIsVBScript)
            {
                if (FAILED(hr = pss->pWriter->ConstOut ("\"", 1)))
                {
                    return hr;
                }
            }
            while (pszLine < pszScan)
            {
                while (*pszLine == '\"')
                {
                    if (bIsVBScript)
                    {
                        if (pszLine + 1 < pszScan)
                        {
                            hr = pss->pWriter->ConstOut ("Chr(34) & ", 10);
                        }
                        else
                        {
                            hr = pss->pWriter->ConstOut ("Chr(34)", 7);
                        }
                    }
                    else
                    {
                        hr = pss->pWriter->ConstOut ("\\\"", 2);
                    }
                    if (FAILED(hr))
                    {
                        return hr;
                    }
                    pszLine++;
                }

                LPCSTR psz;
                for (psz = pszLine; psz < pszScan && *psz != '\"'; psz++)
                {
                }
                if (psz > pszLine)
                {
                    if (bIsVBScript)
                    {
                        if (FAILED(hr = pss->pWriter->ConstOut ("\"", 1)))
                        {
                            return hr;
                        }
                    }
                    if (FAILED(hr = pss->pWriter->Out (pszLine, psz - pszLine)))
                    {
                        return hr;
                    }
                    if (bIsVBScript)
                    {
                        if (psz < pszScan)
                        {
                            hr = pss->pWriter->ConstOut ("\" & ",  4);
                        }
                        else
                        {
                            hr = pss->pWriter->ConstOut ("\"", 1);
                        }
                        if (FAILED(hr))
                        {
                            return hr;
                        }
                    }
                }
                pszLine = psz;
            }
            if (!bIsVBScript)
            {
                if (FAILED(hr = pss->pWriter->ConstOut ("\"", 1)))
                {
                    return hr;
                }
            }
        }
        else
        {
            // Simple output.

            if (FAILED(hr = pss->pWriter->ConstOut ("\"", 1)))
            {
                return hr;
            }
            if (FAILED(hr = pss->pWriter->Out (pszBegin, pszScan - pszBegin)))
            {
                return hr;
            }
            if (FAILED(hr = pss->pWriter->ConstOut ("\"", 1)))
            {
                return hr;
            }
        }

        if (FAILED(hr = pss->pWriter->Out (pss->lang.pszWriteSuffix, 
                                           pss->lang.nWriteSuffixLength)))
        {
            return hr;
        }

        // Only continue when we were interrupted due to a new line.

        if (*pszScan == '\x0d')
        {
            pszScan++;
            if (*pszScan == '\x0a')
            {
                pszScan++;
            }
        }
        else
        {
            break;
        }
    }
    pss->pszPos = pszScan;
    return S_OK;
}

// ============================================================================
// CheckRespMatch
//      Look for and handle Response object method invocations like
//      Write, Clear, End, Flush.
bool
CheckRespMatch(LPCSTR pszScan,
               LPCSTR methodName,
               int len)

{
    return CHAR_TO_UPPER(*pszScan) == methodName[0] && 
        StringMatch(pszScan, methodName, len);
}

// ============================================================================
// WriteRespMatch
//      Write out Respose. method converted to XMLServDoc. method
HRESULT
WriteRespMatch(LPCSTR newMethodName,
               int newLen,
               int oldLen,
               SCANSTATE *pss,
               LPCSTR     pszScan,
               LPCSTR    *ppszBegin)
{
    HRESULT hr;

    // Found a Response.Write, change it to
    // XMLServDoc.Write.
    hr = pss->pWriter->Out(*ppszBegin, pszScan - *ppszBegin);
    HRCHECK(FAILED(hr));

    hr = pss->pWriter->Out(newMethodName, newLen);
    HRCHECK(FAILED(hr));

    *ppszBegin = pszScan + oldLen;

    hr = S_OK;
  Error:
    return hr;
}
// ============================================================================
// ConvertDirective
//      Converts an ASP directive.
//
//      - Processing directives <% %> have their delimiting tags removed.
//      - Output directives <%= xxxx %> are converted to XMLServDoc.Write xxxx
//      - And directives <%@ %> are embedded, as-is, in the code.
//      - Calls to Response.Write are converted to XMLServDoc.Write
//      - Special code handles comments in embedded code.
//      Returns HRESULT indicating success or failure.

HRESULT
ConvertDirective(
    SCANSTATE* pss)                         // Preprocessor state
{
    ASSERT (pss->pszPos[0] == '<');
    ASSERT (pss->pszPos[1] == '%');

    HRESULT hr;
    int nQuoteState = 0;
    bool bCommentMode = false;

    LPCSTR pszBegin;
    if (pss->dwInputState == PPF_IN_PROCESSING_DIRECTIVE)
    {
        pszBegin = pss->pszPos + 2;
    }
    else
    {
        // Write out prologue before section.

        for (pszBegin = pss->pszPos + 2; CHAR_IS_WHITESPACE (*pszBegin); pszBegin++)
        {
        }
        pszBegin++;
        if (pss->dwInputState == PPF_IN_OUTPUT_DIRECTIVE)
        {
            hr = pss->pWriter->Out (pss->lang.pszWritePrefix,
                                    pss->lang.nWritePrefixLength);
            HRCHECK(FAILED(hr));
        }
        else // if (pss->dwInputState == PPF_IN_AT_DIRECTIVE)
        {
            ASSERT (pss->dwInputState == PPF_IN_AT_DIRECTIVE);
            hr = pss->pWriter->ConstOut ("%>\r\n<%@ ", 8);
            HRCHECK(FAILED(hr));
        }
    }

    bool bIsVBScript;
    bIsVBScript = pss->lang.bIsVBScript;
    LPCSTR pszScan;

    for (pszScan = pszBegin; *pszScan != '\0'; pszScan++)
    {
        if (*pszScan == '\x0d')
        {
            bCommentMode = false;
        }
        else if (*pszScan == '\'')
        {
            // In VBScript, the single quote starts a comment. In
            // JScript, it can be used for quotes.

            if (bIsVBScript && nQuoteState == 0)
            {
                bCommentMode = true;
            }
            else if (!bIsVBScript && !bCommentMode)
            {
                nQuoteState = QUOTE_SINGLE_QUOTE_TRANSITION (nQuoteState);
            }
        }
        else if (*pszScan == '\"' && !bCommentMode)
        {
            nQuoteState = QUOTE_DOUBLE_QUOTE_TRANSITION (nQuoteState);
        }
        else if (*pszScan == '/' && !bIsVBScript && nQuoteState == 0 && pszScan[1] == '/')
        {
            // JSScript comments starting with //
            bCommentMode = true;
        }
        else if (*pszScan == '%' && nQuoteState == 0 && pszScan[1] == '>')
        {
            // End of directive.
            break;
        }
        else if (pss->dwInputState == PPF_IN_PROCESSING_DIRECTIVE &&
                 nQuoteState == 0 && !bCommentMode) {

            if (CheckRespMatch(pszScan, GLOBALSTRING(ResponseWrite),
                               GLOBALSTRING_LEN(ResponseWrite))) {

                // Response.Write
                hr = WriteRespMatch(GLOBALSTRING(XMLServDocWriteGeneric), 
                                    GLOBALSTRING_LEN(XMLServDocWriteGeneric),
                                    GLOBALSTRING_LEN(ResponseWrite),
                                    pss, pszScan, &pszBegin);
                HRCHECK(FAILED(hr));

            } else if (CheckRespMatch(pszScan, GLOBALSTRING(ResponseFlush),
                                      GLOBALSTRING_LEN(ResponseFlush))) {

                // Response.Flush
                hr = WriteRespMatch(GLOBALSTRING(XMLServDocFlushGeneric), 
                                    GLOBALSTRING_LEN(XMLServDocFlushGeneric),
                                    GLOBALSTRING_LEN(ResponseFlush),
                                    pss, pszScan, &pszBegin);
                HRCHECK(FAILED(hr));
            } else if (CheckRespMatch(pszScan, GLOBALSTRING(ResponseClear),
                                      GLOBALSTRING_LEN(ResponseClear))) {

                // Response.Clear
                hr = WriteRespMatch(GLOBALSTRING(XMLServDocClearGeneric), 
                                    GLOBALSTRING_LEN(XMLServDocClearGeneric),
                                    GLOBALSTRING_LEN(ResponseClear),
                                    pss, pszScan, &pszBegin);
                HRCHECK(FAILED(hr));
            } else if (CheckRespMatch(pszScan, GLOBALSTRING(ResponseEnd),
                                      GLOBALSTRING_LEN(ResponseEnd))) {

                // Response.End
                hr = WriteRespMatch(GLOBALSTRING(XMLServDocEndGeneric), 
                                    GLOBALSTRING_LEN(XMLServDocEndGeneric),
                                    GLOBALSTRING_LEN(ResponseEnd),
                                    pss, pszScan, &pszBegin);
                HRCHECK(FAILED(hr));
            }

        }

    }

    hr = pss->pWriter->Out (pszBegin, pszScan - pszBegin);
    HRCHECK(FAILED(hr));

    // Skip over %>
    if (*pszScan != '\0')
    {
        pszScan += 2;
    }
    else
    {
        // Error - shouldn't have an unclosed ASP directive.
        RETURNERR(E_FAIL);
    }

    // Write out epilogue.

    if (pss->dwInputState == PPF_IN_PROCESSING_DIRECTIVE)
    {
        // Make sure we add a newline here, because code from two lines
        // (without a delimiter) can get merged into one and cause syntax
        // errors.

        if (bIsVBScript)
        {
            hr = pss->pWriter->ConstOut ("\r\n", 2);
            HRCHECK(FAILED(hr));
        }
        else
        {
            hr = pss->pWriter->ConstOut (";\r\n", 3);
            HRCHECK(FAILED(hr));
        }
    }
    else
    {
        if (pss->dwInputState == PPF_IN_OUTPUT_DIRECTIVE)
        {
            hr = pss->pWriter->Out (pss->lang.pszWriteSuffix, 
                                    pss->lang.nWriteSuffixLength);
            HRCHECK(FAILED(hr));
        }
        else // if (pss->dwInputState == PPF_IN_AT_DIRECTIVE)
        {
            ASSERT (pss->dwInputState == PPF_IN_AT_DIRECTIVE);
            hr = pss->pWriter->ConstOut (" %><%\r\n", 7);
            HRCHECK(FAILED(hr));
        }
    }

    pss->pszPos = pszScan;
    pss->dwInputState = PPF_IN_TEXT;
    
    hr = S_OK;
  Error:
    return hr;
}

// ============================================================================
// ConvertScriptCodeBlock
//      Converts a server OBJECT block. The block is written out as-is.
//      Converts a block of code (a server SCRIPT or OBJECT tag). The
//      block is written out as-is. For SCRIPT tags, calls to Response.Write
//      are converted to XMLServDoc.Write.
//      Returns HRESULT indicating success or failure.

HRESULT
ConvertScriptCodeBlock(
    SCANSTATE* pss)                         // Preprocessor state
{
    ASSERT (pss->pszPos[0] == '<');

    LPCSTR pszBegin = pss->pszPos;
    HRESULT hr;

    if (FAILED(hr = pss->pWriter->ConstOut ("%>\r\n", 4)))
    {
        return hr;
    }

    LPCSTR pszScan;
    bool bIsVBScript = pss->lang.bIsVBScript;
    int nQuoteState = 0;
    bool bCommentMode = false;
    for (pszScan = pss->pszProcessingHint; *pszScan != '\0'; pszScan++)
    {
        if (*pszScan == '\x0d')
        {
            bCommentMode = false;
        }
        else if (*pszScan == '\'')
        {
            // In VBScript, the single quote starts a comment. In
            // JScript, it can be used for quotes.

            if (bIsVBScript && nQuoteState == 0)
            {
                bCommentMode = true;
            }
            else if (!bIsVBScript && !bCommentMode)
            {
                nQuoteState = QUOTE_SINGLE_QUOTE_TRANSITION (nQuoteState);
            }
        }
        else if (*pszScan == '\"' && !bCommentMode)
        {
            nQuoteState = QUOTE_DOUBLE_QUOTE_TRANSITION (nQuoteState);
        }
        else if (*pszScan == '/' && !bIsVBScript && nQuoteState == 0 && pszScan[1] == '/')
        {
            // JSScript comments starting with //
            bCommentMode = true;
        }
        else if (*pszScan == '<' && pszScan[1] == '/' && 
                CHAR_TO_UPPER (pszScan[2]) == GLOBALSTRING(ScriptName)[0])
        {
            // Found </SCRIPT> tag?
            LPCSTR pszTagEnd;
            if (StringMatchWithDelim (pszScan + 2, GLOBALSTRING(ScriptName), 
                    GLOBALSTRING_LEN(ScriptName), '>', &pszTagEnd))
            {
                pszScan = pszTagEnd + 1;
                break;
            }
        }
        else if (CHAR_TO_UPPER (*pszScan) == GLOBALSTRING(ResponseWrite)[0] && 
            nQuoteState == 0 && !bCommentMode &&
            StringMatch (pszScan, GLOBALSTRING(ResponseWrite), GLOBALSTRING_LEN(ResponseWrite)))
        {
            // Found a Response.Write, change it to XMLServDoc.Write.
            if (FAILED(hr = pss->pWriter->Out (pszBegin, pszScan - pszBegin)))
            {
                return hr;
            }
            if (FAILED(hr = pss->pWriter->Out (GLOBALSTRING(XMLServDocWriteGeneric), 
                                               GLOBALSTRING_LEN(XMLServDocWriteGeneric))))
            {
                return hr;
            }
            pszBegin = pszScan + GLOBALSTRING_LEN(ResponseWrite);
        }
    }

    if (FAILED(hr = pss->pWriter->Out (pszBegin, pszScan - pszBegin)))
    {
        return hr;
    }
    if (FAILED(hr = pss->pWriter->ConstOut ("<%\r\n", 4)))
    {
        return hr;
    }

    pss->pszPos = pszScan;
    pss->dwInputState = PPF_IN_TEXT;
    return S_OK;
}

// ============================================================================
// ConvertObjectCodeBlock
//      Converts a server OBJECT block. The block is written out as-is.
//      Returns HRESULT indicating success or failure.

HRESULT
ConvertObjectCodeBlock(
    SCANSTATE* pss)                         // Preprocessor state.
{
    ASSERT (pss->pszPos[0] == '<');

    HRESULT hr;

    if (FAILED(hr = pss->pWriter->ConstOut ("%>\r\n", 4)))
    {
        return hr;
    }
    
    LPCSTR pszScan;
    for (pszScan = pss->pszProcessingHint; *pszScan != '\0'; pszScan++)
    {
        if (*pszScan == '<' && pszScan[1] == '/' && 
                CHAR_TO_UPPER (pszScan[2]) == GLOBALSTRING(ObjectName)[0])
        {
            // Found </OBJECT> tag?
            LPCSTR pszTagEnd;
            if (StringMatchWithDelim (pszScan + 2, GLOBALSTRING(ObjectName), 
                    GLOBALSTRING_LEN(ObjectName), '>', &pszTagEnd))
            {
                pszScan = pszTagEnd + 1;
                break;
            }
        }
    }

    if (FAILED(hr = pss->pWriter->Out (pss->pszPos, pszScan - pss->pszPos)))
    {
        return hr;
    }
    if (FAILED(hr = pss->pWriter->ConstOut ("<%\r\n", 4)))
    {
        return hr;
    }

    pss->pszPos = pszScan;
    pss->dwInputState = PPF_IN_TEXT;
    return S_OK;
}

