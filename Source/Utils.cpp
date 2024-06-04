// ============================================================================
// FILE: Utils.cpp
//
//    Description: Common utility code.
//
// Copyright (c) 1999-2000 Microsoft Corporation.  All rights reserved.

#include "StdAfx.h"

////////////////////////////////////////////////////////////////////////

// Lookup table array, to return whether a character is a whitespace or not.
                                  
const BYTE g_bIsWhitespace[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /* tab */, 1 /* \r */, 0, 0, 1 /* \n */, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    1 /* space */, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
};

// Lookup table array, to return the uppercase equivalent of a character.
// Only returns a different value for lowercase alphabetic characters.

const char g_cUpperChar[] =
{
    '\x00','\x01','\x02','\x03','\x04','\x05','\x06','\x07',
    '\x08','\x09','\x0a','\x0b','\x0c','\x0d','\x0e','\x0f',
    '\x10','\x11','\x12','\x13','\x14','\x15','\x16','\x17',
    '\x18','\x19','\x1a','\x1b','\x1c','\x1d','\x1e','\x1f',
    '\x20','\x21','\x22','\x23','\x24','\x25','\x26','\x27',
    '\x28','\x29','\x2a','\x2b','\x2c','\x2d','\x2e','\x2f',
    '\x30','\x31','\x32','\x33','\x34','\x35','\x36','\x37',
    '\x38','\x39','\x3a','\x3b','\x3c','\x3d','\x3e','\x3f',
    '\x40','\x41','\x42','\x43','\x44','\x45','\x46','\x47',
    '\x48','\x49','\x4a','\x4b','\x4c','\x4d','\x4e','\x4f',
    '\x50','\x51','\x52','\x53','\x54','\x55','\x56','\x57',
    '\x58','\x59','\x5a','\x5b','\x5c','\x5d','\x5e','\x5f',
    '\x60','\x41','\x42','\x43','\x44','\x45','\x46','\x47', // a
    '\x48','\x49','\x4a','\x4b','\x4c','\x4d','\x4e','\x4f', // -
    '\x50','\x51','\x52','\x53','\x54','\x55','\x56','\x57', // z
    '\x58','\x59','\x5a','\x7b','\x7c','\x7d','\x7e','\x7f',
    '\x80','\x81','\x82','\x83','\x84','\x85','\x86','\x87',
    '\x88','\x89','\x8a','\x8b','\x8c','\x8d','\x8e','\x8f',
    '\x90','\x91','\x92','\x93','\x94','\x95','\x96','\x97',
    '\x98','\x99','\x9a','\x9b','\x9c','\x9d','\x9e','\x9f',
    '\xa0','\xa1','\xa2','\xa3','\xa4','\xa5','\xa6','\xa7',
    '\xa8','\xa9','\xaa','\xab','\xac','\xad','\xae','\xaf',
    '\xb0','\xb1','\xb2','\xb3','\xb4','\xb5','\xb6','\xb7',
    '\xb8','\xb9','\xba','\xbb','\xbc','\xbd','\xbe','\xbf',
    '\xc0','\xc1','\xc2','\xc3','\xc4','\xc5','\xc6','\xc7',
    '\xc8','\xc9','\xca','\xcb','\xcc','\xcd','\xce','\xcf',
    '\xd0','\xd1','\xd2','\xd3','\xd4','\xd5','\xd6','\xd7',
    '\xd8','\xd9','\xda','\xdb','\xdc','\xdd','\xde','\xdf',
    '\xe0','\xe1','\xe2','\xe3','\xe4','\xe5','\xe6','\xe7',
    '\xe8','\xe9','\xea','\xeb','\xec','\xed','\xee','\xef',
    '\xf0','\xf1','\xf2','\xf3','\xf4','\xf5','\xf6','\xf7',
    '\xf8','\xf9','\xfa','\xfb','\xfc','\xfd','\xfe','\xff'
};

// Lookup table to handle quote state transitions. Allows nesting of
// single quotes inside double quotes and vice versa. 
//              States: 0=No Quotes, 1=Outer single quote, 2=Outer double quote.
const int g_nSingleQuoteTransition[] = { 1, 0, 2 };
const int g_nDoubleQuoteTransition[] = { 2, 1, 0 };

// ============================================================================
// StringMatch
//              Basic case-insensitive string matching function. Like a strnicmp,
//              but works with ASCII strings only.
//              Returns true if matched, false otherwise.

bool
StringMatch(
    LPCSTR pszString,                       // String to check
    LPCSTR pszMatchUpperCase,               // String to match to (must be all caps)
    int nMatchChars)                        // Number of chars in pszMatchUpperCase
{
    ASSERT (pszString != NULL);
    ASSERT (pszMatchUpperCase != NULL);
    ASSERT (nMatchChars > 0);

    while (nMatchChars > 0 && CHAR_TO_UPPER (*pszString) == *pszMatchUpperCase)
    {
        pszString++;
        pszMatchUpperCase++;
        nMatchChars--;
    }
    return nMatchChars == 0;
}

// ============================================================================
// StringMatchWord
//              Case-insensitive string matching function, that checks if we
//              are matching a whole word. If the strings match, it verifies that
//              next character in the string is a whitespace.
//              Returns true if matched, false otherwise.

bool
StringMatchWord(
    LPCSTR pszString,                       // String to check
    LPCSTR pszMatchUpperCase,               // String to match to (must be all caps)
    int nMatchChars)                        // Number of chars in pszMatchUpperCase
{
    ASSERT (pszString != NULL);
    ASSERT (pszMatchUpperCase != NULL);
    ASSERT (nMatchChars > 0);

    while (nMatchChars > 0 && CHAR_TO_UPPER (*pszString) == *pszMatchUpperCase)
    {
        pszString++;
        pszMatchUpperCase++;
        nMatchChars--;
    }
    return nMatchChars == 0 && 
        (*pszString == '\0' || CHAR_IS_WHITESPACE (*pszString));
}

// ============================================================================
// StringMatchWithDelim
//              Case-insensitive string matching function, that checks for a proper
//              trailing delimiter. If the strings match, it verifies that the 
//              following characters in the string are an arbitrary number of whitespaces
//              followed by the given delimiter character.
//              The function will also return the location of the delimiter character,
//              if matched properly.
//              Returns true if matched, false otherwise.

bool
StringMatchWithDelim(
    LPCSTR pszString,                       // String to check
    LPCSTR pszMatchUpperCase,               // String to match to (must be all caps)
    int nMatchChars,                        // Number of chars in pszMatchUpperCase
    char cDelimChar,                        // Delimiter character
    LPCSTR* ppszDelim)                      // Receives pointer to delimiter on match
{
    ASSERT (pszString != NULL);
    ASSERT (pszMatchUpperCase != NULL);
    ASSERT (nMatchChars > 0);
    ASSERT (cDelimChar != '\0');
    ASSERT (ppszDelim != NULL);

    while (nMatchChars > 0 && CHAR_TO_UPPER (*pszString) == *pszMatchUpperCase)
    {
        pszString++;
        pszMatchUpperCase++;
        nMatchChars--;
    }

    if (nMatchChars > 0)
    {
        return false;
    }

    // Matched string, now skip whitespaces, and check delimiter character.

    while (CHAR_IS_WHITESPACE (*pszString))
    {
        pszString++;
    }
    *ppszDelim = pszString;
    return *pszString == cDelimChar;
}

// ============================================================================
// FindAttributeInTag
//      Finds the value of an attribute in a tag. The function looks for
//      strings in the form 
//              attr = "value"  or  attr = 'value'  or  attr = value
//      with any arbitrary amount of whitespace surrounding the equal sign.
//      The search terminates once the given tag end character is reached.
//      For HTML tags, this should be '>'. For ASP directives, this should be
//      '%'. The function properly handles occurrences of the tag end
//      character within quotes. 
//      
//      The function can optionally find the end of the tag as well. If
//      ppszTagEnd is not NULL, it will receive the location where the tag
//      end character occurs. Doing this will add execution time to the function.
//
//      If there are multiple occurrences of the given attribute, only the
//      first value will be returned.
//
//      Returns pointer to attribute value if matched, NULL otherwise. If the
//      attribute value is quoted, a pointer to the inner contents will be 
//      returned. Other information may be returned in pnValueLen and ppszTagEnd.

LPCSTR 
FindAttributeInTag(
    LPCSTR pszScan,                         // String to scan
    LPCSTR pszAttr,                         // Attribute name (must be all caps)
    int nAttrLen,                           // Length of attribute name
    int* pnValueLen,                        // Receives length of value
    char cTagEndChar,                       // Tag End character (see above)
    LPCSTR* ppszTagEnd)                     // [optional] Receives end of tag position
{
    ASSERT (pszScan != NULL);
    ASSERT (pszAttr != NULL);
    ASSERT (nAttrLen > 0);
    ASSERT (pnValueLen != NULL);
    ASSERT (cTagEndChar != '\0');

    char c = pszAttr[0];
    BYTE bLastWasWhiteSpace = true;
    int nQuoteState = 0;
    LPCSTR pszFound = NULL;

    for (;;)
    {
        // Skip over whitespaces.
        if (CHAR_IS_WHITESPACE (*pszScan))
        {
            bLastWasWhiteSpace = true;
            do
            {
                pszScan++;
            }
            while (CHAR_IS_WHITESPACE (*pszScan));
        }

        if (*pszScan == '\0' || (*pszScan == cTagEndChar && nQuoteState == 0))
        {
            // Couldn't find attribute.
            if (ppszTagEnd != NULL)
            {
                *ppszTagEnd = pszScan;
            }
            return NULL;
        }
        else if (*pszScan == '\'')
        {
            // Enter single quote state, or handle single quote inside double quote.
            nQuoteState = QUOTE_SINGLE_QUOTE_TRANSITION (nQuoteState);
        }
        else if (*pszScan == '\"')
        {
            // Enter double quote state, or handle double quote inside single quote.
            nQuoteState = QUOTE_DOUBLE_QUOTE_TRANSITION (nQuoteState);
        }
        else if (CHAR_TO_UPPER(*pszScan) == c && bLastWasWhiteSpace &&
                 StringMatchWithDelim (pszScan, pszAttr, nAttrLen, '=', &pszFound))
        {
            // Found the attribute.
            break;
        }
        bLastWasWhiteSpace = false;
        pszScan++;
    }

    // Found attribute, now figure out value, first skipping over whitespaces.

    for (pszFound = pszFound + 1; CHAR_IS_WHITESPACE (*pszFound); pszFound++);
    if (*pszFound == '\'' || *pszFound == '\"')
    {
        // Scan forward until quote is closed or end of string.
        char cEndDelim = *pszFound;
        pszFound++;
        for (pszScan = pszFound; 
             *pszScan != '\0' && *pszScan != cEndDelim; 
             pszScan++)
        {
        }
        *pnValueLen = pszScan - pszFound;
        if (*pszScan != '\0')
        {
            pszScan++;
        }
    }
    else
    {
        // Scan forward until next whitespace or end of string.
        for (pszScan = pszFound;
             *pszScan != '\0' && !CHAR_IS_WHITESPACE (*pszScan) && *pszScan != cTagEndChar;
             pszScan++)
        {
        }
        *pnValueLen = pszScan - pszFound;
    }

    // If requested, we need to find the end of the tag. This is a 
    // simpler version of the loop above - it doesn't bother to check
    // for the attribute.

    if (ppszTagEnd != NULL)
    {
        nQuoteState = 0;
        while (*pszScan != '\0')
        {
            if (*pszScan == cTagEndChar && nQuoteState == 0)
            {
              break;
            }
            else if (*pszScan == '\'')
            {
                nQuoteState = QUOTE_SINGLE_QUOTE_TRANSITION (nQuoteState);
            }
            else if (*pszScan == '\"')
            {
                nQuoteState = QUOTE_DOUBLE_QUOTE_TRANSITION (nQuoteState);
            }
            pszScan++;
        }
        
        *ppszTagEnd = pszScan;
    }

    return pszFound;
}

#if _DEBUG
namespace xslisapi {
    int SetBreakpoint() {
        int setBreakpointHere = 33;
        return setBreakpointHere;
    }
}
#endif

// ============================================================================
// FUNCTION: GetStylesheetPIContentsFromXMLDocument
//      Extract the contents of the <?xml-stylesheet...?> processing
//      instruction, and places the result in the incoming CComBSTR
//      pointer.  Don't fill in the BSTR if there's none found.

HRESULT
GetStylesheetPIContentsFromXMLDocument(
    IXMLDOMDocument *pDocument,   // [in] doc to grab from
    CComBSTR & bstrStylesheetPIContents) // [out] bstr to fill in
{
    HRESULT hr;
    CComBSTR bstrStylesheetTag;
    CComPtr<IXMLDOMNode> pcomStylesheetNode;
    CComPtr<IXMLDOMNode> pcomNextNode;
    CComPtr<IXMLDOMProcessingInstruction> pcomStylesheetInstruction;

    static const WCHAR wszXMLStylesheetTag[] = L"xml-stylesheet";

    ASSERT(pDocument != NULL);

    hr = pDocument->get_firstChild(&pcomNextNode);
    HRCHECK(FAILED(hr));

    for(;;)
    {
        // didn't find xml-stylesheet tag
        if (!pcomNextNode || S_FALSE == hr) {
            RETURNERR(S_OK);
        }

        pcomStylesheetNode = pcomNextNode;
        pcomNextNode.Release();

        ASSERT(NULL == bstrStylesheetTag.m_str);
        hr = pcomStylesheetNode->get_nodeName(&bstrStylesheetTag);
        HRCHECK(FAILED(hr));

        if (0 == lstrcmp(bstrStylesheetTag, wszXMLStylesheetTag)) {
            break;              // found it
        }

        hr = pcomStylesheetNode->get_nextSibling(&pcomNextNode);
        HRCHECK(FAILED(hr));

        bstrStylesheetTag.Empty();
    }

    hr = pcomStylesheetNode.QueryInterface(&pcomStylesheetInstruction);
    if (E_NOINTERFACE == hr) {
        // Bail out if this isn't a PI.
        RETURNERR(S_OK);
    }
    HRCHECK(FAILED(hr));

    hr = pcomStylesheetInstruction->get_data(&bstrStylesheetPIContents);
    HRCHECK(FAILED(hr));

    hr = S_OK;
  Error:
    return hr;
}

// ============================================================================
// FUNCTION: ReallyLoadXMLDocument
//      Unconditionally load filename into pDocument.  Any errors are
//      passed on to pRequester->SetError().
HRESULT
ReallyLoadXMLDocument(
    IXMLDOMDocument *pDocument,   // doc to load to
    wchar_t *pwszFilename,               // filename of file to load
    wchar_t *pwszURL,                    // URL for filename (for error reporting)
    bool     bIsHTTPPath,                // is this an http:// path
    CXMLServerDocument *pRequester)      // propagate errors here.
{
    HRESULT      hr;
    VARIANT_BOOL result;

    if (!bIsHTTPPath) {
        // Ensure file exists and give a better message than the XML
        // parse error would give.
        WIN32_FIND_DATA data;
        HANDLE h = FindFirstFile(pwszFilename, &data);
        bool filefound = (h != INVALID_HANDLE_VALUE);
        FindClose(h);
        if (!filefound) {
            pRequester->SetError(L"Resource not found",
                                 pwszURL,
                                 L"404 Not Found");
            RETURNERR(E_FAIL);
        }
    }

    hr = pDocument->put_async(VARIANT_FALSE);
    HRCHECK(FAILED(hr));

    hr = pDocument->put_validateOnParse(VARIANT_FALSE);
    HRCHECK(FAILED(hr));

    // Note that due to Q237906
    // (http://support.microsoft.com/support/kb/articles/Q237/9/06.ASP) the
    // following isn't supported and may not work when pwszFilename is
    // a URL for an external machine (i.e., starts with http://)
    hr = pDocument->load(CComVariant(pwszFilename),
                         &result);
    HRCHECK(FAILED(hr));
    
    if (hr != S_OK || result == VARIANT_FALSE) {

        HRESULT parseErrorCode;
        hr = DealWithParseError(pDocument,
                                pwszURL,
                                pRequester,
                                &parseErrorCode);
        HRCHECK(FAILED(hr));

        hr = parseErrorCode;
        HRCHECK(FAILED(hr));
    }

    hr = S_OK;
  Error:
    return hr;
}


// ============================================================================
// FUNCTION: GetSingleNodeValue
//      Given a XMLDOMNode and a XPath (XSL pattern), look for the first matched
//      node and retrieve its value in string.
//      Returned string variable will be set to NULL if no node is matched.
HRESULT
GetSingleNodeValue(IXMLDOMNode *pNode,
                   LPCWSTR pwszXPath,
                   BSTR *pbstrValue)
{
    HRESULT hr;
    CComBSTR bstrNodeValue;
    CComPtr<IXMLDOMNode> pcomNode;

    ASSERT(pNode && pwszXPath && pbstrValue);

    hr = pNode->selectSingleNode(const_cast<LPWSTR> (pwszXPath),
                                 &pcomNode);
    HRCHECK(FAILED(hr));

    if (pcomNode.p != NULL) {

        CComVariant var;
        hr = pcomNode->get_nodeValue(&var);
        HRCHECK(FAILED(hr));
                
        hr = var.ChangeType(VT_BSTR);
        HRCHECK(FAILED(hr));

        bstrNodeValue = V_BSTR(&var);
    }

    *pbstrValue = bstrNodeValue.Detach();

    hr = S_OK;
  Error:
    return hr;
}

// ============================================================================
// CreateXMLDocumentOnCComPtr
//     Create an MSXML3 or an MSXML free-threaded document (if MSXML3
//     isn't available) on the incoming CComPtr.
HRESULT
CreateXMLDocumentOnCComPtr(CComPtr<IXMLDOMDocument> & pcomDoc)
{
    HRESULT hr;

    if (g_xml3Availability == xml3AvailabilityUnchecked ||
        g_xml3Availability == xml3AvailabilityAvailable) {

        // Create the MSXML3 document.  
        hr = pcomDoc.CoCreateInstance(CLSID_FreeThreadedDOMDocument);
        if (g_xml3Availability == xml3AvailabilityUnchecked) {
            g_xml3Availability =
                SUCCEEDED(hr) ? xml3AvailabilityAvailable
                              : xml3AvailabilityUnavailable;
        } else {

            // If we thought XML3 was available, this had better have
            // succeeded. 
            HRCHECK(FAILED(hr));
        }
    }

    ASSERT(g_xml3Availability != xml3AvailabilityUnchecked);
    if (g_xml3Availability == xml3AvailabilityUnavailable) {
        hr = pcomDoc.CoCreateInstance(my_CLSID_DOMFreeThreadedDocument);
        HRCHECK(FAILED(hr));
    }

    ASSERT(pcomDoc.p);

    hr = S_OK;
  Error:
    return hr;
}

// ============================================================================
// DealWithParseError
//      If there's a parse error on the document, invoke
//      pRequester->SetError() and put the error code into *pXmlHR.
//      Else make *pXmlHR == S_OK.  Note that this function expects
//      there to have been a parse error.  If there wasn't, this will
//      return bad results.

HRESULT
DealWithParseError(IXMLDOMDocument *pDocument,
                   wchar_t *pwszURL,
                   CXMLServerDocument *pRequester,
                   HRESULT *pXmlHR)
{
    HRESULT hr;
    CComPtr<IXMLDOMParseError> pcomParseError;
    CComBSTR                   bstrErrorInfo;

    *pXmlHR = S_OK;
    hr = pDocument->get_parseError(&pcomParseError);
    HRCHECK(FAILED(hr));

    hr = pcomParseError->get_errorCode(pXmlHR);
    HRCHECK(FAILED(hr));
        
    if (pcomParseError.p) {
        hr = pcomParseError->get_reason(&bstrErrorInfo);
        HRCHECK(FAILED(hr));

        pRequester->SetError(bstrErrorInfo,
                             pwszURL,
                             L"500.100 Internal Server Error - ASP Error");
    }

    hr = S_OK;
  Error:
    return hr;
}



// ============================================================================
// GetASPServerObject
//      Gets the ASP server object.
//      Returns HRESULT indicating success.

HRESULT 
GetASPServerObject(
    asp::IServer** ppServer)                  // [out] Receives server object
{
    HRESULT hr;
    CComPtr<IObjectContext> pcomObjectContext;
    CComPtr<IGetContextProperties> pcomProps;
    CComPtr<asp::IServer> pcomServer;
    VARIANT vt;

    VariantInit(&vt);

    hr = ::GetObjectContext(&pcomObjectContext);
    HRCHECK (FAILED(hr));

    hr = pcomObjectContext.QueryInterface(&pcomProps);
    HRCHECK (FAILED(hr));

    hr = pcomProps->GetProperty(g_bstrServer, &vt);
    HRCHECK(FAILED(hr));
    ERRCHECK (V_VT(&vt) != VT_DISPATCH || V_DISPATCH(&vt) == NULL, E_FAIL);

    hr = V_DISPATCH(&vt)->QueryInterface (asp::IID_IServer, 
            reinterpret_cast<PVOID*>(ppServer));
    HRCHECK (FAILED(hr));

    hr = S_OK;
  Error:
    VariantClear (&vt);
    return hr;
}

// ============================================================================
// GrabFromBrowserCap
//   Grab the value of pwszAttrib from the browser capabilities object
//   and store as a string in bstrDestination.  If none exists (or is
//   "unknown") replace with pwszDefault (or just empty if NULL).
HRESULT
GrabFromBrowserCap(IDispatch *pBrowserTypeDisp,
                   wchar_t *pwszAttrib,
                   wchar_t *pwszDefault,
                   CComBSTR & bstrDestination)
{
    HRESULT     hr;
    CComVariant var;

    bstrDestination.Empty();
    
    hr = GetBrowserTypeProperty(pBrowserTypeDisp, pwszAttrib, var);
    if (hr != S_OK) {
        // Assume no content-type was specified here, use default.
        if (pwszDefault) {
            bstrDestination = pwszDefault;
        }
    } else {
        hr = var.ChangeType(VT_BSTR);
        HRCHECK(FAILED(hr));
        if (0 == lstrcmp(V_BSTR(&var), L"unknown")) {
            if (pwszDefault) {
                bstrDestination = pwszDefault;
            }
        } else {
            bstrDestination = V_BSTR(&var);
        }
    }

    hr = S_OK;
  Error:
    return hr;
}
    
// ============================================================================
// GetBrowserTypeProperty
//      Get property from browser type or S_FALSE if no such property.
HRESULT
GetBrowserTypeProperty(
    IDispatch *pBrowserTypeDisp,  // [in] IDispatch of the BrowserType object
    const WCHAR *pwszPropertyName,  // [in] Property name to get
    CComVariant & vPropertyValue  // [out] Reference to resulting CComVariant
)
{
    HRESULT hr;
    DISPID dispid = 0;
    DISPPARAMS dispParams;

    // Dummy parameter for the Invoke call below since the implementation of it
    // on NT4/IIS4 seems to access the pointer without checking if it is NULL.
    ZeroMemory(&dispParams, sizeof(dispParams));

    ASSERT(NULL != pBrowserTypeDisp);
    ASSERT(NULL != pwszPropertyName);

    vPropertyValue.Clear();

    hr = pBrowserTypeDisp->GetIDsOfNames(
        IID_NULL,
        const_cast<WCHAR**> (&pwszPropertyName),
        1,
        0,
        &dispid);
    HRCHECK(FAILED(hr));

    hr = pBrowserTypeDisp->Invoke(
        dispid,
        IID_NULL,
        0,
        DISPATCH_PROPERTYGET,
        &dispParams,
        &vPropertyValue,
        NULL,
        NULL);
    HRCHECK(FAILED(hr));

    // NT4 and W2K returns the string "unknown" in different cases ("Unknown"
    // and "unknown respectively), so a case-insensitive comparison is needed.
    if (VT_EMPTY == V_VT(&vPropertyValue) ||
        (VT_BSTR == V_VT(&vPropertyValue) &&
         !lstrcmpi(L"unknown", V_BSTR(&vPropertyValue)))) {
        hr = S_FALSE;
    }

  Error:
    return hr;
}


////////////////////////
// String conversions
////////////////////////

// This "new"s the result.  Caller is responsible for calling
// 'delete[]' 
char *
WideToAscii(const WCHAR* wsz)
{
    int len = ::WideCharToMultiByte(CP_ACP, 0, wsz, -1, NULL, 0, 0, 0);
    char* psz = new char[len+1];
    ::WideCharToMultiByte(CP_ACP, 0, wsz, -1, psz, len, 0, 0);
    psz[len] = 0;
    return psz;
}
