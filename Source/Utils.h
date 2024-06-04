// ============================================================================
// FILE: Utils.h
//
//    Description: Header file defining common utility code and common
//    error handling and debugging macros.
//
// Copyright (c) 1999-2000 Microsoft Corporation.  All rights reserved.

#pragma once

////////////////////////
//
// Debugging
//
////////////////////////


#if defined(_DEBUG)

// disable "conditional expr is constant" warning.  Frequently fires
// for asserts.
#pragma warning(disable : 4127) 

#define ASSERT(exp) _ASSERTE(exp)
#define VERIFY(exp) ASSERT(exp)
#define DEBUG_CODE(exp) exp

#else  // Not _DEBUG

#define ASSERT(exp) (__assume(exp))
#define VERIFY(exp) (exp)
#define DEBUG_CODE(exp)

#endif

// ASSERT_VALID_BSTR - a macro to help check if a BSTR passed in is valid.
//      Not guaranteed, but it does (usually) catch string constants, etc.
//      that are passed in instead of BSTRs.

#define ASSERT_VALID_BSTR(bstr) ASSERT(((bstr) == NULL) || (SysStringLen (bstr) == (UINT)lstrlenW (bstr)))

// ============================================================================
// STRUCT UTILITIES
//      Various utility functions to work on structs, and arrays of structs.

// ============================================================================
// LookupSortedArray
//      Templatized function to look up a key in a sorted array. Similar
//      to CRT bsearch function, but this one's templatized.
//      
//      To call this function, you need to implement a callback function
//      that compares the key with an element in the array, and returns
//          <0 if *pKey is less than *pElem
//          0 if *pKey is equal to *pElem
//          >0 if *pKey is greater than *pElem
//
//      Returns index to element if found, or -1.

template <typename Element, typename Key> 
int
LookupSortedArray(
    Key const* pKey,                        // [in] Pointer to key
    Element const* pElemArray,              // [in] Pointer to array
    int nElemCount,                         // [in] Number of elements in array
    int (__fastcall* pfnCompare)(Key const* pKey, Element const* pElem))
                                            // [in] Callback (see above)
{
    int nLeft = 0;
    int nRight = nElemCount - 1;
    int nFind = -1;
    while (nLeft <= nRight)
    {
        int nPivot = (nLeft + nRight) / 2;
        int nCmp = pfnCompare (pKey, pElemArray + nPivot);
        if (nCmp == 0)
        {
            nFind = nPivot;
            break;
        }
        else if (nCmp < 0)
        {
            nRight = nPivot - 1;
        }
        else if (nCmp > 0)
        {
            nLeft = nPivot + 1;
        }
    }
    return nFind;
}
        
// ============================================================================
// VerifySortedArray
//      Debug-mode templatized function to verify that an array 
//      of elements is properly sorted.
//      
//      To call this function, you need to implement a callback function
//      that compares an element with another, and returns
//          <0 if *pElem1 is less than *pElem2
//          0 if *pElem1 is equal to *pElem2
//          >0 if *pElem1 is greater than *pElem2
//
//      Returns true if sorted, false otherwise.

template <typename Element>
bool 
VerifySortedArray(
    Element const* pElemArray,              // [in] Pointer to array
    int nElemCount,                         // [in] Number of elements in array
    int (__fastcall* pfnCompare)(Element const* pElem1, Element const* pElem2))
                                            // [in] Callback (see above)
{
    ASSERT (nElemCount == 0 || pElemArray != NULL);

    for (int i = 1; i < nElemCount; i++)
    {
        if (pfnCompare (pElemArray + i - 1, pElemArray + i) > 0)
            break;
    }

    return (0 == nElemCount) || (i == nElemCount);
}

#define SAFERELEASE(ptr)   \
  if (ptr != NULL) {       \
      ptr->Release();      \
      ptr = NULL;          \
  }

#define SAFEADDREF(pUnk)   \
  if (pUnk != NULL) {      \
     pUnk->AddRef();       \
  }



// Arrays for fast character checks.
   
extern const BYTE g_bIsWhitespace[];
extern const char g_cUpperChar[];
extern const int g_nSingleQuoteTransition[];
extern const int g_nDoubleQuoteTransition[];

// CHAR_IS_WHITESPACE - returns 1 if the character is a whitespace, 0 otherwise

#define CHAR_IS_WHITESPACE(c) (g_bIsWhitespace[c] != 0)

// CHAR_TO_UPPER - returns uppercase version of character

#define CHAR_TO_UPPER(c) (g_cUpperChar[c])

// Quote transition macros - help determine quote status in a document,
// allowing nesting of single quotes inside double quotes, or vice versa.
// To process quotes, start with state = 0, and call these macros whenever
// you see a quote character. When state = 0, the current state is unquoted.

#define QUOTE_SINGLE_QUOTE_TRANSITION(curstate) (g_nSingleQuoteTransition[curstate])
#define QUOTE_DOUBLE_QUOTE_TRANSITION(curstate) (g_nDoubleQuoteTransition[curstate]) 

// Macros for declaring constant strings, and getting their value and size.

#define DECLARE_GLOBALSTRING(x, val) \
    const char g_sz##x[] = val; \
    const int g_n##x##_size = sizeof(g_sz##x) - 1
#define GLOBALSTRING(x) g_sz##x
#define GLOBALSTRING_LEN(x) g_n##x##_size

// String search functions.

bool __cdecl StringMatch(LPCSTR pszString, LPCSTR pszMatchUpperCase, int nMatchChars);
bool __cdecl StringMatchWord(LPCSTR pszString, LPCSTR pszMatchUpperCase, int nMatchChars);
bool __cdecl StringMatchWithDelim(LPCSTR pszString, LPCSTR pszMatchUpperCase, int nMatchChars, char cDelimChar, LPCSTR* ppszTerm);
LPCSTR __cdecl FindAttributeInTag(LPCSTR pszScan, LPCSTR pszAttr, int nAttrLen, int* pnValueLen, char cTagEndChar, LPCSTR* ppszTagEnd);

////////////////////////
// ASP Related
////////////////////////

HRESULT GetASPServerObject(asp::IServer **ppServer);

// Grab the value of pwszAttrib from the browser capabilities object
// and store as a string in bstrDestination.  If none exists (or is
// "unknown") replace with pwszDefault (or leave untouched if NULL).
HRESULT GrabFromBrowserCap(IDispatch *pBrowserTypeDisp,
                           wchar_t *pwszAttrib,
                           wchar_t *pwszDefault,
                           CComBSTR & bstrDestination);

// Get property from browser type or S_FALSE if no such property.
HRESULT GetBrowserTypeProperty(IDispatch *pBrowserTypeDisp,
                               const WCHAR *pwszPropertyName,
                               CComVariant & vPropertyValue);

////////////////////////
// String conversions
////////////////////////

// This "new"s the result.  Caller is responsible for calling
// 'delete[]' 
char* WideToAscii(const WCHAR* wsz);

////////////////////////
// Preprocessor Related
////////////////////////

HRESULT CreateProcessingStream(IStream * pOutputStream,
                               asp::IResponse * pResponse,
                               const WCHAR * pwszStreamLanguage,
                               UINT uiCP,
                               IStream ** ppProcessingStream);

////////////////////////
// XML Related
////////////////////////

HRESULT GetStylesheetPIContentsFromXMLDocument(IXMLDOMDocument *pDocument,
                                               CComBSTR & pbstrStylesheetPIContents);

// Load filename into pDocument.  Any errors are passed on to
// pRequester. 
HRESULT ReallyLoadXMLDocument(IXMLDOMDocument *pDocument,
                              wchar_t *pwszFilename,
                              wchar_t *pwszURL,
                              bool     bIsHTTPPath,
                              CXMLServerDocument *pRequester);

HRESULT DealWithParseError(IXMLDOMDocument *pDocument,
                           wchar_t *pwszURL,
                           CXMLServerDocument *pRequester,
                           HRESULT *pXmlHR);

HRESULT GetSingleNodeValue(IXMLDOMNode *pNode,
                           LPCWSTR pwszXPath,
                           BSTR *pbstrValue);

// Construct either an MSXML or an MSXML3 free-threaded document,
// favoring MSXML3 if available.
HRESULT CreateXMLDocumentOnCComPtr(CComPtr<IXMLDOMDocument> & pcomDoc);
                                           

////////////////////////
// Error handling
////////////////////////

#if _DEBUG

#define ERRORTRACE(hr)                                         \
    if (S_OK != hr) {                                          \
        _RPTF2(                                                \
            _CRT_WARN,                                         \
            "XSLISAPI Error -- thread id: %d, hr: 0x%lx\n",    \
            GetCurrentThreadId(),                              \
            hr);                                               \
    }

namespace xslisapi {
    int SetBreakpoint();
}

#define ALLOWBREAK xslisapi::SetBreakpoint();

#else  // !_DEBUG

#define ALLOWBREAK
#define ERRORTRACE(hr)

#endif // _DEBUG

#define RETURNERR(hrToReturn) \
  {                     \
      hr = hrToReturn;  \
      ALLOWBREAK        \
      ERRORTRACE(hr);   \
      goto Error;       \
  }

#define ERRCHECK(cond, hrToReturn) \
  if (cond) {           \
      hr = hrToReturn;  \
      ALLOWBREAK        \
      ERRORTRACE(hr);   \
      goto Error;       \
  }

#define HRCHECK(cond) \
  if (cond) {         \
      ALLOWBREAK      \
      ERRORTRACE(hr); \
      goto Error;     \
  }


