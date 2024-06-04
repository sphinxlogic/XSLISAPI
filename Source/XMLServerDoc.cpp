// ============================================================================
// FILE: XMLServerDoc.cpp
//
//        Implementation of server-side object that gathers XML data and
//        invokes XSL processing on it.
//
// Copyright (c) 1999-2000 Microsoft Corporation. All rights reserved.

#include "StdAfx.h"
#include "XMLServerDoc.h"
#include "charset.h"

// ============================================================================
// CXMLServerDocument::WriteLine
//      Add line to current XML buffer.

STDMETHODIMP
CXMLServerDocument::WriteLine(
    BSTR bstrLine)                          // [in] Line to add to buffer
{
    return WriteToXML (bstrLine, true);
}


// ============================================================================
// CXMLServerDocument::Write
//      Add text to current XML buffer.

STDMETHODIMP
CXMLServerDocument::Write(
    BSTR bstrLine)                          // [in] Line to add to buffer
{
    return WriteToXML (bstrLine, false);
}


// ============================================================================
// CXMLServerDocument::End
//      Delegate this method to the ASP Response object

STDMETHODIMP
CXMLServerDocument::End()
{
    m_bResponseEndCalled = true;
    return S_OK;
}


// ============================================================================
// CXMLServerDocument::Flush
//      Handle "flushing" the XML Server Document

STDMETHODIMP
CXMLServerDocument::Flush()
{
    // Just ignore flushes, since we haven't yet written to the
    // Response object yet anyhow.
    return S_OK;
}


// ============================================================================
// CXMLServerDocument::Clear
//      Clear out the XML document by releasing its stream and
//      ensuring that a new one is opened.

STDMETHODIMP
CXMLServerDocument::Clear()
{
    m_pcomXMLDocumentStream.Release();
    return EnsureXMLDocumentObject(true);
}


// ============================================================================
// CXMLServerDocument::Load
//      Loads the XML document from a file 
//      Returns HRESULT indicating success.

STDMETHODIMP
CXMLServerDocument::Load(BSTR bstrFileName)   // [in] local filename to load
{
    HRESULT hr;

    // If End() has already been called, just skip over this.  (Unless
    // we're somehow in error handling, in which case any previous
    // call to End() should be ignored.)
    if (m_bResponseEndCalled && !m_bInErrorHandling) {
        RETURNERR(S_OK);
    }

    ClearError();

    // Make sure document is created and acquired.
    hr = EnsureXMLDocumentObject (false);
    HRCHECK(FAILED(hr));

    hr = ReallyLoadXMLDocument(m_pcomXMLDocument,
                               bstrFileName,
                               m_bstrURL,
                               false,
                               this);
    HRCHECK(FAILED(hr));

    hr = S_OK;
  Error:
    return hr;
}


// ============================================================================
// CXMLServerDocument::Transform
//      Transform the accumulated XML document.

STDMETHODIMP
CXMLServerDocument::Transform(
    IDispatch * pdispResponse)              // [in] Response stream
{
    HRESULT hr;
    const int MAX_SHEETS_TO_CHAIN = 64;
    CComPtr<asp::IResponse>             pcomResponse;
    CComPtr<IXMLDOMDocument>            pcomServerConfig;
    CComBSTR                            bstrStylesheets[MAX_SHEETS_TO_CHAIN];
    short                               nStylesheets = 0;
    CComBSTR                            bstrPIContents;
    CComBSTR                            bstrSpecialPIAttrib;

    ClearError();

    ERRCHECK (pdispResponse == NULL, E_POINTER);

    // Make sure we've been given a real response object.
    hr = pdispResponse->QueryInterface (
            asp::IID_IResponse,
            reinterpret_cast<void **>(&pcomResponse));
    HRCHECK (FAILED(hr));

    // If we've been writing to a stream, release it
    if (m_pcomXMLDocumentStream.p) {
        m_pcomXMLDocumentStream.Release();
    }

    hr = GetDoctype();
    HRCHECK(FAILED(hr));

    hr = ::GetStylesheetPIContentsFromXMLDocument(m_pcomXMLDocument,
                                                  bstrPIContents);
    HRCHECK(FAILED(hr));

    if (bstrPIContents.Length() == 0) {
        // There are no PI contents, just bail out with the original.
        hr = WriteIdentityXML(pcomResponse);
        RETURNERR(hr);
    }

    hr = m_piParseInfo.Clear();
    HRCHECK(FAILED(hr));

    hr = m_piParseInfo.Parse(bstrPIContents);
    HRCHECK(FAILED(hr));

    hr = LoadMasterConfig(bstrSpecialPIAttrib);
    HRCHECK(FAILED(hr));
    
    hr = InitializeBrowserCapAndAttribs();
    HRCHECK(FAILED(hr));

    hr = GetServerConfig(&pcomServerConfig);
    HRCHECK(FAILED(hr));

// BEGIN BACK COMPAT
    
    // Maintain backward compatability with XLSISAPI v1.1 by, if there
    // is no server-config, looking for either the bstrSpecialPIAttrib
    // and then "server-href" in the PI contents.  If found, use the
    // referenced stylesheet.  If not, just pass the XML back out and
    // return.  
    if (pcomServerConfig.p == NULL) {

        // When there's no server-config, just set the configDirectory to 
        // the directory of the URL, since backwards-compat stylesheets will 
        // be specified relative to that.
        m_bstrConfigDirectory = m_bstrURLDirectory;

        wchar_t *stylesheetForBackwardCompat;

        stylesheetForBackwardCompat = m_piParseInfo.Find(bstrSpecialPIAttrib);
        if (!stylesheetForBackwardCompat) {
            stylesheetForBackwardCompat = m_piParseInfo.Find(L"server-href");
        }

        if (stylesheetForBackwardCompat) {
            bstrStylesheets[0] = stylesheetForBackwardCompat;
            nStylesheets = 1;
        }

// END BACK COMPAT
        
    } else {

        // We do have a server-config we're working with.
        nStylesheets = COUNTOF(bstrStylesheets);
        hr = ExtractStylesheets(pcomServerConfig,
                                bstrStylesheets,
                                &nStylesheets);
        HRCHECK(FAILED(hr));

    }

    if (nStylesheets == -1 || nStylesheets == 0) {
        // Either no <device> element matched (-1), or one did, but it 
        // didn't have any <stylesheet> children (0).  In this case,
        // just pass the XML back out and return.  (May also get here
        // if there is no server-config, or invocations due to
        // backwards compatability.)
        hr = WriteIdentityXML(pcomResponse);
        RETURNERR(hr);
    }

    hr = ApplyStylesheets(pcomResponse,
                          bstrStylesheets,
                          nStylesheets);
    HRCHECK(FAILED(hr));
    
    hr = S_OK;
  Error:
    return hr;
}

// ============================================================================
// CXMLServerDocument::GetDoctype
//      Pull the doctype from the document
HRESULT
CXMLServerDocument::GetDoctype()
{
    HRESULT hr;
    CComPtr<IXMLDOMDocumentType> pcomDoctype;

    hr = m_pcomXMLDocument->get_doctype(&pcomDoctype);
    HRCHECK(FAILED(hr));

    if (pcomDoctype.p != NULL) {
        hr = pcomDoctype->get_name(&m_bstrDoctypeName);
        HRCHECK(FAILED(hr));
    }

    hr = S_OK;
  Error:
    return hr;
}

// ============================================================================
// CXMLServerDocument::EnsureXMLDocumentObject
//      Makes sure that the XML Document object is created, and all 
//      interfaces we want from it have been acquired.
//      Returns HRESULT indicating success.

HRESULT
CXMLServerDocument::EnsureXMLDocumentObject(
    bool bAcquireStream)              // [in] Acquire interface for streamed
                                      //      input
{
    const WCHAR chBOM = L'\xFEFF'; // Byte-order mark
    HRESULT hr;

    // Create XML document.
    if (!m_pcomXMLDocument.p) {

        hr = CreateXMLDocumentOnCComPtr(m_pcomXMLDocument);
        HRCHECK(FAILED(hr));
    }

    // Get a stream interface, if desired.
    if (bAcquireStream && !m_pcomXMLDocumentStream.p) {
        hr = m_pcomXMLDocument.QueryInterface(&m_pcomXMLDocumentStream);
        HRCHECK(FAILED(hr));

        // Write out BOM.
        hr = m_pcomXMLDocumentStream->Write(&chBOM, sizeof(chBOM), NULL);
        HRCHECK(FAILED(hr));
    }

    hr = S_OK;
  Error:
    return hr;
}

// ============================================================================
// CXMLServerDocument::EnsureAspServerObject
//      Makes sure that the AspServer object is created
HRESULT
CXMLServerDocument::EnsureAspServerObject()
{
    HRESULT hr;
    
    if (m_pcomASPServer.p == NULL) {
        hr = ::GetASPServerObject(&m_pcomASPServer);
        HRCHECK(FAILED(hr));
    }

    hr = S_OK;
  Error:
    return hr;
}

// ============================================================================
// CXMLServerDocument::WriteToXML
//      Writes data, with optional carriage return, to XML buffer.
//      Returns HRESULT indicating success.

HRESULT 
CXMLServerDocument::WriteToXML(
    BSTR bstrLine,                          // [in] String to write
    bool bAddCR)                            // [in] Add carriage return?
{
    HRESULT hr = S_OK;
    
    ASSERT_VALID_BSTR (bstrLine);

    // If End() has already been called, just skip over this.  (Unless
    // we're somehow in error handling, in which case any previous
    // call to End() should be ignored.)
    if (m_bResponseEndCalled && !m_bInErrorHandling) {
        RETURNERR(S_OK);
    }

    if ((NULL != bstrLine && L'\0' != *bstrLine) || bAddCR)
    {
        hr = EnsureXMLDocumentObject (true);
        HRCHECK (FAILED(hr));

        if (NULL != bstrLine && L'\0' != *bstrLine) {
            hr = m_pcomXMLDocumentStream->Write(
                bstrLine,
                lstrlen(bstrLine) * sizeof(bstrLine[0]),
                NULL);
            HRCHECK(FAILED(hr));
        }
        
        if (bAddCR) {
            hr = m_pcomXMLDocumentStream->Write(
                L"\r\n",
                2 * sizeof(WCHAR),
                NULL);
            HRCHECK(FAILED(hr));
        }
    }

    hr = S_OK;
  Error:
    return hr;
}

// ============================================================================
// CXMLServerDocument::put_URL
//      Sets the URL.
//      Returns HRESULT indicating success.

STDMETHODIMP
CXMLServerDocument::put_URL(
    BSTR bstrURL)                           // [in] URL
{
    HRESULT hr = S_OK;

    ClearError();

    m_bstrURL = bstrURL;
    ERRCHECK(m_bstrURL.m_str == NULL, E_OUTOFMEMORY);

    m_bstrURLDirectory.Empty();
    hr = g_fileSystemObject->GetParentFolderName(bstrURL,
                                                 &m_bstrURLDirectory);
    HRCHECK(FAILED(hr));

    hr = S_OK;
  Error:
    return hr;
}

// ============================================================================
// CXMLServerDocument::put_UserAgent
//      Sets UserAgent string.
//      Returns HRESULT indicating success.
// BEGIN BACK COMPAT
STDMETHODIMP
CXMLServerDocument::put_UserAgent(
    BSTR bstrUserAgent)                           // [in] UserAgent
{
    HRESULT hr = S_OK;

    ClearError();

    m_bstrUserAgent = bstrUserAgent;
    ERRCHECK(m_bstrUserAgent.m_str == NULL, E_OUTOFMEMORY);

    hr = S_OK;
  Error:
    return hr;
}
// END BACK COMPAT

// ============================================================================
// CXMLServerDocument::WriteIdentityXML
//      Simply write out the XML document to the response (setting the
//      content type along the way)

HRESULT
CXMLServerDocument::WriteIdentityXML(asp::IResponse *pResponse)
{
    HRESULT hr;
    
    hr = pResponse->put_ContentType(L"text/xml");
    HRCHECK(FAILED(hr));

    // It's possible we got here because of a bad XML document.  If
    // attempting to save here returns an error it may be that there's
    // a parse error.  Check for it. 
    hr = m_pcomXMLDocument->save(CComVariant(pResponse));
    if (FAILED(hr)) {
        HRESULT parseError;
        
        hr = DealWithParseError(m_pcomXMLDocument,
                                m_bstrURL,
                                this,
                                &parseError);
        HRCHECK(FAILED(hr));

        hr = parseError;
        HRCHECK(FAILED(hr));
    }

    hr = S_OK;
  Error:
    return hr;
}


// BEGIN BACK COMPAT

// ============================================================================
// CXMLServerDocument::LoadMasterConfig
//      Loads in the /xslisapi/masterConfig.xml file.  If there's a
//      special ProcessingInstruction attribute to use based upon the
//      <client> element matching the user-agent string, set it.

HRESULT
CXMLServerDocument::LoadMasterConfig(CComBSTR & bstrSpecialPIAttrib)
{
    HRESULT                  hr;
    CComPtr<IXMLDOMDocument> pcomMasterConfig;
    CComPtr<IXMLDOMNodeList> pcomClientNodes;
    CComPtr<IXMLDOMNode>     pcomClientNode;
    CComBSTR                 tempStr;
    bool                     foundMatch;
    wchar_t                  pwszConfigFilename[] = L"/xslisapi/masterConfig.xml";

    if (m_bInErrorHandling) {
        // Don't do anything with the master config when we're
        // handling an error.
        RETURNERR(S_OK);
    }
    
    // TODO: Note that this entire processing doesn't really need to
    // happen for each transform that occurs.  But since it's mostly
    // for backwards compatability with version 1.1, we don't optimize
    // it.  Also, it's not so bad because the config file is likely to
    // be in the cache.
    
    hr = LoadXMLFromRelativeLoc(pwszConfigFilename,
                                NULL,
                                false,
                                &pcomMasterConfig,
                                NULL);

    // This config file is optional.
    if (FAILED(hr)) {
        ClearError();
        RETURNERR(S_OK);
    }

    // Deal with cache timeout
    hr = GetSingleNodeValue(pcomMasterConfig,
                            L"/config/cache/@cleanup",
                            &tempStr);
    HRCHECK(FAILED(hr));

    hr = g_xmlCache->SetMinutes(_wtoi(tempStr));
    HRCHECK(FAILED(hr));
                            
    // Look for encoding if it hasn't been set
    if (!m_bstrEncoding.Length()) {
        hr = GetSingleNodeValue(pcomMasterConfig,
                                L"/config/output/@encoding",
                                &m_bstrEncoding);
        HRCHECK(FAILED(hr));
    }

    // Deal with <client> elements
    hr = pcomMasterConfig->selectNodes(L"/config/client",
                                       &pcomClientNodes);
    HRCHECK(FAILED(hr));

    foundMatch = false;
    hr = pcomClientNodes->nextNode(&pcomClientNode);
    HRCHECK(FAILED(hr));

    while (!foundMatch && pcomClientNode.p != NULL) {

        tempStr.Empty();
        hr = GetSingleNodeValue(pcomClientNode,
                                L"@name",
                                &tempStr);
        HRCHECK(FAILED(hr));

        if (wcsstr(m_bstrUserAgent, tempStr)) {

            hr = GetSingleNodeValue(pcomClientNode,
                                    L"@href",
                                    &bstrSpecialPIAttrib);
            HRCHECK(FAILED(hr));

            if (bstrSpecialPIAttrib.m_str == NULL) {
                SetError(L"Matching <client> element found without 'href' attribute",
                         pwszConfigFilename,
                         L"500.100 Internal Server Error - ASP Error");
                RETURNERR(E_FAIL);
            }
            
            foundMatch = true;
            
        }
        
        pcomClientNode.Release();
        hr = pcomClientNodes->nextNode(&pcomClientNode);
        HRCHECK(FAILED(hr));
    }

    hr = S_OK;
  Error:
    return hr;
}
// END BACK COMPAT

// ============================================================================
// CXMLServerDocument::GetServerConfig
//      Pulls the server-config attribute out of the <?xml-stylesheet>
//      processing instruction, if there is one.  Else returns NULL.
HRESULT
CXMLServerDocument::GetServerConfig(IXMLDOMDocument **ppServerConfig)
{
    HRESULT   hr;
    CComBSTR  bstrPIContents;

    wchar_t *serverConfigFilename = m_piParseInfo.Find(L"server-config");

    if (serverConfigFilename == NULL) {
        
        *ppServerConfig = NULL;
        
    } else {

        m_bstrURLServerConfig = serverConfigFilename;
        ERRCHECK(m_bstrURLServerConfig.m_str == NULL, E_OUTOFMEMORY);
        
        hr = LoadXMLFromRelativeLoc(m_bstrURLServerConfig,
                                    m_bstrURLDirectory,
                                    true,
                                    ppServerConfig,
                                    NULL);
        HRCHECK(FAILED(hr));
    }

    hr = S_OK;
  Error:
    return hr;
}

enum PathDisposition {
    pathDispositionRelativePath,
    pathDispositionAbsolutePath,
    pathDispositionAbsolutePathWithDriveSpec,
    pathDispositionHttpPath
};

// ============================================================================
// GetPathDisposition
//     Figure out the type of path the parameter represents.
//
PathDisposition
GetPathDisposition(BSTR localName)
{
    PathDisposition pathDisposition;
    const wchar_t   wszHttpPrefix[] = L"http://";
    const UINT      nHttpPrefix = COUNTOF(wszHttpPrefix);
    
    if (SysStringLen(localName) >= nHttpPrefix &&
        wcsncmp(localName, wszHttpPrefix, nHttpPrefix-1) == 0) {

        // http:// path
        pathDisposition = pathDispositionHttpPath;
        
    } else if (localName[0] == L'/' ||
               localName[0] == L'\\') {

        // absolute local path
        pathDisposition = pathDispositionAbsolutePath;

    } else if (localName[1] == L':' &&
               (localName[2] == L'/' ||
                localName[2] == L'\\')) {

        // absolute local path with drive spec
        pathDisposition = pathDispositionAbsolutePathWithDriveSpec;
            
    } else {

        // relative local path
        pathDisposition = pathDispositionRelativePath;
    }

    return pathDisposition;
    
}

    
// ============================================================================
// CXMLServerDocument::LoadXMLFromRelativeLoc
//     Loads in an XML document from a file specification, possibly
//     relative to the provided path.  XML document is created,
//     loaded, and returned.
HRESULT
CXMLServerDocument::LoadXMLFromRelativeLoc(
    BSTR localName,             // [in] specified local name
    BSTR pathName,              // [in] relative to this path
    bool isConfigXML,           // [in] true when loading
                                // server-config only.

    // Only one of the next two will actually be loaded.  If the
    // incoming ppTemplate != NULL, we'll see if we can create an XSL
    // Template out of whatever we find.
    IXMLDOMDocument **ppXMLDoc,   // [out] XML document that's been
                                  // populated.  May have been in
                                  // cache. 
    IXSLTemplate    **ppTemplate  // [out] XSL template that's been
                                  // populated.  May have been in
                                  // cache. 
    )
{
    HRESULT hr;
    CComBSTR                 bstrResolvedPath;
    CComBSTR                 bstrServerMappedPath;
    CComPtr<IXMLDOMDocument> pcomXMLDoc;
    char                    *pszServerMappedPath;
    bool                     bIsHTTPPath = false;

    ASSERT(*ppXMLDoc == NULL);
    ASSERT(!ppTemplate || (*ppTemplate == NULL));

    switch (GetPathDisposition(localName)) {
      case pathDispositionHttpPath:
      case pathDispositionAbsolutePath:
        bstrResolvedPath = localName;
        break;

      case pathDispositionAbsolutePathWithDriveSpec:
        SetError(L"File specifications with drive references are not allowed.",
                 localName,
                 L"403.2 - Forbidden - Read Access Forbidden");
        RETURNERR(E_FAIL);
        break;

      case pathDispositionRelativePath:
        hr = g_fileSystemObject->BuildPath(pathName,
                                           localName,
                                           &bstrResolvedPath);
        HRCHECK(FAILED(hr));
        break;

      default:
        RETURNERR(E_FAIL);
        break;
    }

    // Only for the server-config xml...
    if (isConfigXML) {
        // Stash away the directory that the server-config file is
        // in.  Stylesheets will be loaded relative to this
        // directory.
        m_bstrConfigDirectory.Empty();
        hr = g_fileSystemObject->GetParentFolderName(bstrResolvedPath,
                                                     &m_bstrConfigDirectory);
        HRCHECK(FAILED(hr));
    }

    hr = EnsureAspServerObject();
    HRCHECK(FAILED(hr));

    switch (GetPathDisposition(bstrResolvedPath)) {
        
      case pathDispositionHttpPath:
        bstrServerMappedPath = bstrResolvedPath;
        break;

      case pathDispositionAbsolutePathWithDriveSpec:
        SetError(L"File specifications with drive references are not allowed.",
                 localName,
                 L"403.2 - Forbidden - Read Access Forbidden");
        RETURNERR(E_FAIL);
        break;

      case pathDispositionAbsolutePath:
      case pathDispositionRelativePath:
        hr = m_pcomASPServer->MapPath(bstrResolvedPath,
                                      &bstrServerMappedPath);
        if (FAILED(hr)) {
            SetError(L"Unsupported path specification",
                     localName,
                     L"404 - Not Found");
            RETURNERR(hr);
        }
        break;
        
      default:
        RETURNERR(E_FAIL);
        break;
    }

    // TODO: Could modify cache/hashtable to work over wide strings,
    // so we don't need to convert.
    pszServerMappedPath = WideToAscii(bstrServerMappedPath);

    // Note: ->Lookup does an AddRef().  Note that it also loads the
    // file if not present in the cache, and adds it to the cache.
    // Therefore, the only client call that needs to be made into the
    // cache is Lookup().
    hr = g_xmlCache->Lookup(pszServerMappedPath,
                            localName,
                            bIsHTTPPath,
                            this,
                            ppXMLDoc,
                            ppTemplate);
    delete [] pszServerMappedPath;
    HRCHECK(FAILED(hr));

    hr = S_OK;
  Error:
    return hr;
}


// ============================================================================
// CXMLServerDocument::GetBrowserCap
//      Get the browser capabilities object from the IIS environment
//      and stash away in a member variable.  Also grab the content
//      type, charset and encoding here (if they're specified in
//      browscap.ini) 
HRESULT
CXMLServerDocument::InitializeBrowserCapAndAttribs()
{
    HRESULT hr;

    if (m_pcomBrowserTypeDisp.p == NULL) {

        hr = EnsureAspServerObject();
        HRCHECK(FAILED(hr));
    
        hr = m_pcomASPServer->CreateObject(g_bstrBrowserType, 
                                           &m_pcomBrowserTypeDisp);
        HRCHECK(FAILED(hr));

        hr = GrabFromBrowserCap(m_pcomBrowserTypeDisp,
                                L"content-type",
                                L"text/html",
                                m_bstrContentType);
        HRCHECK(FAILED(hr));

        hr = GrabFromBrowserCap(m_pcomBrowserTypeDisp,
                                L"charset",
                                NULL,
                                m_bstrCharset);
        HRCHECK(FAILED(hr));

        hr = GrabFromBrowserCap(m_pcomBrowserTypeDisp,
                                L"encoding",
                                NULL,
                                m_bstrEncoding);
        HRCHECK(FAILED(hr));

    } else {
        
        // m_pcomASPServer already set, be sure content type is also. 
        ASSERT(m_bstrContentType.m_str != NULL);
        
    }

    hr = S_OK;
  Error:
    return hr;
}


// ============================================================================
// CXMLServerDocument::ExtractStylesheets
//      Pull appropriate stylesheet names out of the <server-config>
//      XML schema based on the browser capabilities object.
//
//      If there are no matches, return -1 in *pNumStylesheets.  Else
//      return the number of array elements filled in by the matching
//      <device> element (could be 0).
HRESULT
CXMLServerDocument::ExtractStylesheets(
    IXMLDOMDocument  *pServerConfig,      // [in] server-config XML document
    CComBSTR          arrStylesheets[],   // [in] array of strings of URLs for XSLs,
                                          // [out] filled in array
    short            *pNumStylesheets)    // [in] ptr to size of array
                                          // [out] ptr to num of array slots filled in. 
{
    HRESULT                  hr;
    CComPtr<IXMLDOMNodeList> pcomDeviceNodes;
    CComPtr<IXMLDOMNode>     pcomDeviceNode;
    bool                     gotStylesheets;

    hr = pServerConfig->selectNodes(L"/server-styles-config/device",
                                    &pcomDeviceNodes);
    HRCHECK(FAILED(hr));

    hr = pcomDeviceNodes->nextNode(&pcomDeviceNode);
    HRCHECK(FAILED(hr));

    // Loop through each device element
    gotStylesheets = false;
    while (!gotStylesheets && pcomDeviceNode.p != NULL) {

        CComPtr<IXMLDOMNamedNodeMap>  pcomAttrs;
        CComPtr<IXMLDOMNode>          pcomAttr;
        bool                          foundMatch;

        hr = pcomDeviceNode->get_attributes(&pcomAttrs);
        HRCHECK(FAILED(hr));

        hr = pcomAttrs->nextNode(&pcomAttr);
        HRCHECK(FAILED(hr));

        // Loop through all the attributes in the device element,
        // comparing to the browser capabilities. 
        foundMatch = true;
        while (pcomAttr.p != NULL) {

            ASSERT(foundMatch);
            
            CComBSTR    propertyName;
            CComVariant varPropertyValue;
            CComVariant varBrowserCapValue;
            
            hr = pcomAttr->get_nodeName(&propertyName);
            HRCHECK(FAILED(hr));

            hr = pcomAttr->get_nodeValue(&varPropertyValue);
            HRCHECK(FAILED(hr));

            // Test the name/value pair against the browser
            // capabilities.
            hr = GetBrowserTypeProperty(m_pcomBrowserTypeDisp,
                                        propertyName,
                                        varBrowserCapValue);

            if (hr != S_OK  || varBrowserCapValue != varPropertyValue) {
                // we interpret any sort of failure here as the
                // property not existing. 
                foundMatch = false;
                break;
            }

            pcomAttr.Release();
            hr = pcomAttrs->nextNode(&pcomAttr);
            HRCHECK(FAILED(hr));
        }

        if (foundMatch) {

            CComBSTR                 bstrTemp;

            // First look for an overriding "content-type" element.
            hr = GetSingleNodeValue(pcomDeviceNode,
                                    L"content-type/@type",
                                    &bstrTemp);
            HRCHECK(FAILED(hr));
            if (bstrTemp) {
                m_bstrContentType = bstrTemp;
                bstrTemp.Empty();
            }

            // Always override encoding in masterConfig.xml if handling error
            hr = GetSingleNodeValue(pcomDeviceNode,
                                    L"output/@encoding",
                                    &bstrTemp);
            HRCHECK(FAILED(hr));
            if (bstrTemp || m_bInErrorHandling) {
                m_bstrEncoding = bstrTemp;
                bstrTemp.Empty();
            }

            // Look for an overriding "charset" element
            hr = GetSingleNodeValue(pcomDeviceNode,
                                    L"output/@charset",
                                    &bstrTemp);
            HRCHECK(FAILED(hr));
            if (bstrTemp) {
                m_bstrCharset = bstrTemp;
                bstrTemp.Empty();
            }

            hr = PullStylesheetsFromDeviceInfo(pcomDeviceNode,
                                               arrStylesheets,
                                               pNumStylesheets);
            HRCHECK(FAILED(hr));

            gotStylesheets = true;
        }

        pcomDeviceNode.Release();
        hr = pcomDeviceNodes->nextNode(&pcomDeviceNode);
        HRCHECK(FAILED(hr));
    }

    if (!gotStylesheets) {
        // Indicate that there were no matches.
        *pNumStylesheets = -1;
    }

    hr = S_OK;
  Error:
    return hr;
}

// ============================================================================
// DoesDoctypeMatch
//      Check a string to see if a doctype string is contained in a
//      list of doctypes.
HRESULT
DoesDoctypeMatch(CComVariant & varDoctypeListInConfig,
                 CComBSTR    & bstrDoctypeNameInDoc,
                 bool        * pResult)
{
    ASSERT(*pResult == false && "Expected to start out as false");
    
    HRESULT       hr;
    CComBSTR      bstrDoctypeListInConfig;

    hr = varDoctypeListInConfig.ChangeType(VT_BSTR);
    HRCHECK(FAILED(hr));

    bstrDoctypeListInConfig = V_BSTR(&varDoctypeListInConfig);
    ERRCHECK(bstrDoctypeListInConfig.m_str == NULL,
             E_OUTOFMEMORY);

    wchar_t *pwEnd;
    wchar_t *pwTokenStart;
    bool     foundIt;
    
    pwEnd = bstrDoctypeListInConfig.m_str +
            bstrDoctypeListInConfig.Length();
    pwTokenStart = bstrDoctypeListInConfig.m_str;
    foundIt = false;

    // Search through the list for the document's doctype.  We could
    // use the CRT function wcstok here, but that brings in too much
    // of the CRT and results in link problems when using _ATL_MIN_CRT
    // as we are here.
    while (!foundIt && pwTokenStart < pwEnd) {

        // Skip whitespace
        while (pwTokenStart < pwEnd &&
               (*pwTokenStart == L' ' || *pwTokenStart == L',' || *pwTokenStart == L'\t')) {
            pwTokenStart++;
        }

        // Skip over token to next whitespace
        wchar_t *pwTokenEnd = pwTokenStart;
        while (pwTokenEnd < pwEnd &&
               (*pwTokenEnd != L' ' && *pwTokenEnd != L',' && *pwTokenEnd != L'\t')) {
            pwTokenEnd++;
        }

        size_t len = pwTokenEnd - pwTokenStart;
        if (len == bstrDoctypeNameInDoc.Length() &&
            (wcsncmp(pwTokenStart, bstrDoctypeNameInDoc, len) == 0)) {

            foundIt = true;
            
        } else {

            // Go on to next token.
            pwTokenStart = pwTokenEnd;
        }
    }

    if (foundIt) {
        *pResult = true;
    }

    hr = S_OK;
  Error:
    return hr;
}

// ============================================================================
// CXMLServerDocument::PullStylesheetsFromDeviceInfo
//      Given that we have the root of the proper device node, yank
//      out the stylesheet names to use.  Will also take "doctype"
//      into account.
//
//      If there are no matches, return -1 in *pNumStylesheets.  Else
//      return the number of array elements filled in by the matching
//      <device> element (could be 0).

HRESULT
CXMLServerDocument::PullStylesheetsFromDeviceInfo(
    IXMLDOMNode  *pDeviceNode,      // [in] node in tree pointing to chosen device
    CComBSTR      arrStylesheets[], // [in] array of strings of URLs for XSLs,
                                    // [out] filled in array
    short        *pNumStylesheets)  // [in] ptr to size of array
                                    // [out] ptr to num of array slots filled in. 
{
    HRESULT hr;
    CComPtr<IXMLDOMNodeList> pcomHREFs;
    CComPtr<IXMLDOMNode>     pcomHREF;
    CComPtr<IXMLDOMNodeList> pcomDoctypeNodes;
    CComPtr<IXMLDOMNode>     pcomDoctypeNode;
    wchar_t                  stylesheetPattern[] = L"stylesheet/@href";
    wchar_t                  doctypePattern[] = L"doctype";
    wchar_t                  doctypeNamePattern[] = L"@name";
    bool                     foundDoctypeMatch = false;
    const short              maxStylesheets = *pNumStylesheets;

    *pNumStylesheets = 0;

    // Grab both the top-level stylesheet/@href nodes and the top
    // level doctype nodes.  They shouldn't both simultaneously
    // exist. 
    hr = pDeviceNode->selectNodes(stylesheetPattern, &pcomHREFs);
    HRCHECK(FAILED(hr));

    hr = pcomHREFs->nextNode(&pcomHREF);
    HRCHECK(FAILED(hr));

    hr = pDeviceNode->selectNodes(doctypePattern, &pcomDoctypeNodes);
    HRCHECK(FAILED(hr));

    hr = pcomDoctypeNodes->nextNode(&pcomDoctypeNode);
    HRCHECK(FAILED(hr));

    if (pcomHREF.p != NULL && pcomDoctypeNode.p != NULL) {
        SetError(L"Cannot have both a stylesheet node and a doctype node contained within a device node",
                 m_bstrURLServerConfig,
                 L"500.100 Internal Server Error - ASP Error");
        RETURNERR(E_FAIL);
    }

    if (pcomDoctypeNode.p != NULL) {

        // Have doctypes, so change our context based upon the doctype
        // match.
        while (!foundDoctypeMatch && pcomDoctypeNode.p != NULL) {

            // Now have the "<doctype>" node, need to get to the
            // "name" attribute of it.  If there is a <doctype>
            // element without a name attribute, consider it to be the
            // default and allow it to match.
            
            CComPtr<IXMLDOMNode> pcomDoctypeNameNode;

            hr = pcomDoctypeNode->selectSingleNode(doctypeNamePattern,
                                                   &pcomDoctypeNameNode);
            HRCHECK(FAILED(hr));

            if (pcomDoctypeNameNode.p == NULL) {

                // <doctype> element without a "name" attribute.
                foundDoctypeMatch = true;
                
            } else if (m_bstrDoctypeName.m_str != NULL) {
            
                CComVariant varDoctypeNameInConfig;
                
                hr = pcomDoctypeNameNode->get_nodeValue(&varDoctypeNameInConfig);
                HRCHECK(FAILED(hr));

                hr = DoesDoctypeMatch(varDoctypeNameInConfig,
                                      m_bstrDoctypeName,
                                      &foundDoctypeMatch);
                HRCHECK(FAILED(hr));
                
            }

            if (!foundDoctypeMatch) {
                pcomDoctypeNode.Release();
                hr = pcomDoctypeNodes->nextNode(&pcomDoctypeNode);
                HRCHECK(FAILED(hr));
            }
        }
    }

    if (foundDoctypeMatch) {
        // This part actually switches the souce of the pcomHREF
        // node to be underneath the matching doctype.
        pcomHREFs.Release();
        hr = pcomDoctypeNode->selectNodes(stylesheetPattern,
                                          &pcomHREFs);
        HRCHECK(FAILED(hr));

        hr = pcomHREFs->nextNode(&pcomHREF);
        HRCHECK(FAILED(hr));
    }

    // Now loop through the stylesheets.
    while (pcomHREF.p != NULL) {

        CComVariant varHREFValue;
                
        // Just grab the value out of here and put it in the
        // results.
        hr = pcomHREF->get_nodeValue(&varHREFValue);
        HRCHECK(FAILED(hr));

        hr = varHREFValue.ChangeType(VT_BSTR);
        HRCHECK(FAILED(hr));

        // Didn't pass in enough stylesheets to fill up.
        if (*pNumStylesheets >= maxStylesheets) {
            SetError(L"Attempting to chain too many stylesheets",
                     m_bstrURL,
                     L"500.100 Internal Server Error - ASP Error");
            RETURNERR(E_FAIL);
        }
                    
        arrStylesheets[*pNumStylesheets] = V_BSTR(&varHREFValue);
        (*pNumStylesheets)++;

        pcomHREF.Release();
        hr = pcomHREFs->nextNode(&pcomHREF);
        HRCHECK(FAILED(hr));
    }

    hr = S_OK;
  Error:
    return hr;
}

// ============================================================================
// CXMLServerDocument::ApplyStylesheets
//      Sequentially apply given stylesheets, writing the final
//      response to provided response object.
HRESULT
CXMLServerDocument::ApplyStylesheets(asp::IResponse *pResponse,
                                     CComBSTR        arrStylesheets[],
                                     short           numStylesheets)
{
    HRESULT hr;
    CComPtr<IStream> pcomResponseStream;
    CComPtr<IStream> pcomProcessedResponseStream;
    UINT uiCP;

    hr = pResponse->put_ContentType(m_bstrContentType);
    HRCHECK(FAILED(hr));

    hr = VerifyEncodingAndCharset(&uiCP);
    HRCHECK(FAILED(hr));

    // Charset should be set in VerifyEncodingAndCharset() if not configured
    ASSERT(m_bstrCharset.Length());
    hr = pResponse->put_CharSet(m_bstrCharset);
    HRCHECK(FAILED(hr));

    // No failure check is needed when retrieving IStream since it is not
    // required, just nice to have it.  Note that the Response object in
    // NT4/IIS4 doesn't support IStream.
    hr = pResponse->QueryInterface(IID_IStream,
                                   reinterpret_cast<void**>(&pcomResponseStream));

    // Both IStream and IResponse interfaces are passed in.  ProcessingStream
    // object will pick the right interface to use.
    hr = CreateProcessingStream(pcomResponseStream,
                                pResponse,
                                m_bstrContentType,
                                uiCP,
                                &pcomProcessedResponseStream);
    HRCHECK(FAILED(hr));

    if (numStylesheets == 0) {
        
        hr = m_pcomXMLDocument->save(CComVariant(pcomProcessedResponseStream));
        HRCHECK(FAILED(hr));
        
    } else {

        // Go through each of the stylesheets, transforming into a new
        // XML document, until the last one, when we transform into
        // the post-processing response stream.
        short                     stylesheetsLeft = numStylesheets;
        short                     stylesheetIndex = 0;
        IXMLDOMDocument          *pSrcDoc = m_pcomXMLDocument;
        CComPtr<IXMLDOMDocument>  pcomXslDoc;
        CComPtr<IXSLTemplate>     pcomXslTemplate;
        CComPtr<IXMLDOMDocument>  pcomDstDocs[2];
        int                       dstDocIndex = 0;
        CComVariant               varDstDoc;

        while (stylesheetsLeft > 0) {

            // Load in XSL
            pcomXslDoc.Release();
            pcomXslTemplate.Release();

            hr = LoadXMLFromRelativeLoc(arrStylesheets[stylesheetIndex],
                                        m_bstrConfigDirectory,
                                        false,
                                        &pcomXslDoc,
                                        &pcomXslTemplate);
            HRCHECK(FAILED(hr));

            if (stylesheetsLeft == 1) {

                // Write to the stream for the last one
                varDstDoc = pcomProcessedResponseStream;
                
            } else {

                // Else write to a DOM document, but make sure the
                // ping-pong buffers we use are set up initially.

                // TODO: This is place where perf can be improved by
                // maintaining a pool of XML document objects for use
                // by multiple threads and instances of this filter.
                if (pcomDstDocs[dstDocIndex].p == NULL) {
                    hr = CreateXMLDocumentOnCComPtr(pcomDstDocs[dstDocIndex]);
                    HRCHECK(FAILED(hr));
                }

                varDstDoc = pcomDstDocs[dstDocIndex];

            }

            if (pcomXslTemplate.p) {

                CComPtr<IXSLProcessor> pcomXslProc;
                VARIANT_BOOL           done;
                bool                   failed = false;
                
                hr = pcomXslTemplate->createProcessor(&pcomXslProc);
                HRCHECK(FAILED(hr));

                hr = pcomXslProc->put_input(CComVariant(pSrcDoc));
                HRCHECK(FAILED(hr));

                hr = pcomXslProc->put_output(varDstDoc);
                HRCHECK(FAILED(hr));

                done = VARIANT_FALSE;
                while (!failed && done == VARIANT_FALSE) {
                    hr = pcomXslProc->transform(&done);
                    if (FAILED(hr)) {
                        failed = true;
                    }
                }
                
            } else {
                
                hr = pSrcDoc->transformNodeToObject(pcomXslDoc,
                                                    varDstDoc);
                
            }
            
            if (FAILED(hr)) {
                SetErrorToLastCOMError(arrStylesheets[stylesheetIndex]);
                RETURNERR(hr);
            }

            // Previous dest is source for the next one...
            pSrcDoc = pcomDstDocs[dstDocIndex];
            
            stylesheetIndex++;
            stylesheetsLeft--;
            dstDocIndex = 1 - dstDocIndex; // toggle between 0 and 1.
            
        }
    }

    hr = pcomProcessedResponseStream->Commit(STGC_DEFAULT);
    HRCHECK(FAILED(hr));

    hr = S_OK;
  Error:
    return hr;
}


// ============================================================================
// CXMLServerDocument::VerifyEncodingAndCharset
//      Check the configuration of encoding and charset according to the
//      following rules:
//  - Both character set and code page(encoding) are optional in the
//    configuration file.
//  - If both a code page and char set are provided, we will validate the code
//    page for "recognized" char sets and return an error if they are incorrect,
//    if the char set is not recognized we assume that the code page is valid. 
//  - If no code page and no char set are provided, UTF-8 will be used for the
//    transformation and charset string "UTF-8" will be sent to the client.
//  - If a char set is provided and no code page is provided, we will look up
//    the char set in our mapping table to determine the codepage.  If the
//    charset is not found in the mapping table we will return an error
//    requiring a code page.  
//  - If a codepage is provided and no char set is provided, the codepage will
//    be used for the transformation and will set the char set as the same
//    string as the code page string
HRESULT
CXMLServerDocument::VerifyEncodingAndCharset(UINT *puiCP)
{
    HRESULT hr;
    UINT uiCPFromEncoding = CP_UNDEFINED;
    UINT uiCPFromCharset = CP_UNDEFINED;
    char *pszEncoding = NULL;

    ASSERT(puiCP);

    // Validate configuration strings first
    if (m_bstrEncoding.Length()) {
        if (!lstrcmpi(m_bstrEncoding, L"utf-8")) {
            uiCPFromEncoding = CP_UTF8;

        } else if (!lstrcmpi(m_bstrEncoding, L"utf-16") ||
                   !lstrcmpi(m_bstrEncoding, L"ucs-2")) {
            uiCPFromEncoding = CP_UTF16;

        } else {
            pszEncoding = WideToAscii(m_bstrEncoding);

            if (!pszEncoding) {
                RETURNERR(E_OUTOFMEMORY);
            }

            if (StringMatch(pszEncoding, "WINDOWS-", 8)) {
                uiCPFromEncoding = atoi(&pszEncoding[8]);

                int test = WideCharToMultiByte(uiCPFromEncoding, 0, L"TEST", 4,
                                               NULL, 0,0,0);
                if (!test) {
                    SetError(L"Invalid code page value",
                             m_bstrURL,
                             L"500.100 Internal Server Error - ASP Error");
                    RETURNERR(E_INVALIDARG);
                }
            } else {
                SetError(L"Invalid encoding string format",
                         m_bstrURL,
                         L"500.100 Internal Server Error - ASP Error");
                RETURNERR(E_INVALIDARG);
            }
        }
        ASSERT(uiCPFromEncoding != CP_UNDEFINED);
    }

    if (m_bstrCharset.Length()) {
        uiCPFromCharset = uiCodePageFromCharset(m_bstrCharset);
    }

    // 4 combinations of the presences of encoding and charset
    if (m_bstrEncoding.Length() && m_bstrCharset.Length()) {

        // Report error only if charset is in mapping table and the code pages
        // are not the same
        if (uiCPFromCharset != CP_UNDEFINED &&
            uiCPFromEncoding != uiCPFromCharset) {
            SetError(L"Inconsistent code page value and charset string",
                     m_bstrURL,
                     L"500.100 Internal Server Error - ASP Error");
            RETURNERR(E_INVALIDARG);
        }
        *puiCP = uiCPFromEncoding;

    } else if (m_bstrEncoding.Length()) {
        m_bstrCharset = (uiCPFromEncoding == CP_UTF16)?L"UTF-16":m_bstrEncoding;
        *puiCP = uiCPFromEncoding;

    } else if (m_bstrCharset.Length()) {
        if (uiCPFromCharset == CP_UNDEFINED) {
            CComBSTR bstrError;

            bstrError = L"Encoding is required for the " \
                        L"unrecognized charset string: ";
            bstrError += m_bstrCharset;
            SetError(bstrError, m_bstrURL,
                     L"500.100 Internal Server Error - ASP Error");
            RETURNERR(E_INVALIDARG);
        }
        *puiCP = uiCPFromCharset;

    } else {
        *puiCP = CP_UTF8;
        m_bstrCharset = L"UTF-8";
    }

    ASSERT(*puiCP != CP_UNDEFINED);
    hr = S_OK;
  Error:
    if (pszEncoding) {
        delete [] pszEncoding;
    }
    return hr;
}


// ============================================================================
// CXMLServerDocument::HandleError
//      Handles errors generated in the process

STDMETHODIMP
CXMLServerDocument::HandleError(
    IDispatch * pdispResponse)              // [in] Response stream
{
    HRESULT                  hr = S_OK;
    CComPtr<asp::IResponse>  pcomResponse;
    VARIANT_BOOL             result;
    long                     len;
    wchar_t                 *buffer;

    const wchar_t *part1 =
        L"<?xml version=\"1.0\"?>"
        L"<?xml-stylesheet type=\"text/xsl\" server-config=\"/xslisapi/errorConfig.xml\"?>"
        L"<error><status-code>";
    
    const wchar_t *part2 = L"</status-code><url>";
    const wchar_t *part3 = L"</url><info><![CDATA[";
    const wchar_t *part4 = L"]]></info></error>";
    const long constantLength = wcslen(part1) + wcslen(part2) +
                                wcslen(part3) + wcslen(part4);

    ERRCHECK (pdispResponse == NULL, E_POINTER);
 
    ASSERT(!m_bInErrorHandling);
    m_bInErrorHandling = true;

    // Will only get here without having an error description set if
    // an unexpected error occurred.
    if (m_bstrErrorDescrip.m_str == NULL) {
        SetError(L"Unanticipated error",
                 L"Unknown URL",
                 L"500.100 Internal Server Error - ASP Error");
    }

    // refill document with error schema
    len = constantLength +
          m_bstrErrorDescrip.Length() +
          m_bstrErrorHTTPCode.Length() +
          m_bstrErrorURL.Length() + 1;

    buffer = static_cast<wchar_t*>(_alloca(len * sizeof(wchar_t)));
    ERRCHECK(buffer == NULL, E_OUTOFMEMORY);
    
    wcscpy(buffer, part1);
    wcscat(buffer, m_bstrErrorHTTPCode);
    wcscat(buffer, part2);
    wcscat(buffer, m_bstrErrorURL);
    wcscat(buffer, part3);
    wcscat(buffer, m_bstrErrorDescrip);
    wcscat(buffer, part4);

    hr = EnsureXMLDocumentObject(false);
    HRCHECK(FAILED(hr));
    
    hr = m_pcomXMLDocument->loadXML(CComBSTR(buffer), &result);
    HRCHECK(FAILED(hr));
    ERRCHECK(result == VARIANT_FALSE, E_FAIL);
    
    // Make sure we've been given a real response object.
    hr = pdispResponse->QueryInterface (
            asp::IID_IResponse,
            reinterpret_cast<void **>(&pcomResponse));
    HRCHECK (FAILED(hr));

    hr = pcomResponse->Clear();
    HRCHECK(FAILED(hr));

    // Now just transform the error information as we would any other
    // XML. 
    hr = Transform(pdispResponse);
    HRCHECK(FAILED(hr));

    hr = S_OK;
  Error:
    ASSERT(m_bInErrorHandling);
    m_bInErrorHandling = false;
    return hr;
}

// ============================================================================
// CXMLServerDocument::ClearError
//      Clears out error structures
STDMETHODIMP
CXMLServerDocument::ClearError()
{
    m_bstrErrorDescrip.Empty();
    m_bstrErrorURL.Empty();
    m_bstrErrorHTTPCode.Empty();

    return S_OK;
}

// ============================================================================
// CXMLServerDocument::SetError
//      Sets error structures.  (Note that it's OK for incoming
//      strings to just be wchar_t*'s.)
STDMETHODIMP
CXMLServerDocument::SetError(BSTR errorDescrip,
                             BSTR errorURL,
                             BSTR errorHTTPCode)
{
    m_bstrErrorDescrip = errorDescrip;
    m_bstrErrorURL = errorURL;
    m_bstrErrorHTTPCode = errorHTTPCode;
    
    return S_OK;
}

HRESULT
CXMLServerDocument::SetErrorToLastCOMError(wchar_t *pwszURL)
{
    HRESULT             hr;
    CComPtr<IErrorInfo> pcomErrInfo;
    CComBSTR            bstrReason;

    hr = ::GetErrorInfo(0, &pcomErrInfo);
    HRCHECK(FAILED(hr));

    hr = pcomErrInfo->GetDescription(&bstrReason);
    HRCHECK(FAILED(hr));

    hr = this->SetError(bstrReason,
                        pwszURL,
                        L"500.100 Internal Server Error - ASP Error");

    HRCHECK(FAILED(hr));

    hr = S_OK;
  Error:
    return hr;
}
