// ============================================================================
// FILE: XMLServerDoc.h
//
//        Implementation of server-side object that gathers XML data and
//        processes it by combining with the appropriate XSL transform.
//
// Copyright (c) 1999-2000 Microsoft Corporation. All rights reserved.

#pragma once

#include "PIParse.h"

// ============================================================================
// CLASS: CXMLServerDocument
//
//      Server-side object that accumulates XML, then transforms it via request-
//      specific XSL.

class ATL_NO_VTABLE CXMLServerDocument : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CXMLServerDocument, &CLSID_XMLServerDocument>,
    public IDispatchImpl<IXMLServerDocument, &IID_IXMLServerDocument, &LIBID_XSLISAPI2Lib>
{
public:

DECLARE_REGISTRY_RESOURCEID(IDR_XMLSERVERDOCUMENT)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CXMLServerDocument)
    COM_INTERFACE_ENTRY(IXMLServerDocument)
    COM_INTERFACE_ENTRY(IDispatch)
END_COM_MAP()

  public:
    CXMLServerDocument() : m_bInErrorHandling(false),
                           m_bResponseEndCalled(false) {}
    HRESULT SetErrorToLastCOMError(wchar_t *pwszURL);

// IXMLServerDocument
    STDMETHOD(put_URL)(/*[in]*/ BSTR bstrURL);
    STDMETHOD(put_UserAgent)(/*[in]*/ BSTR bstrUserAgent);
    STDMETHOD(Transform)(IDispatch * pdispResponse);
    STDMETHOD(HandleError)(IDispatch * pdispResponse);
    STDMETHOD(Load)(BSTR bstrFileName);
    STDMETHOD(Write)(BSTR bstrText);
    STDMETHOD(End)();
    STDMETHOD(Flush)();
    STDMETHOD(Clear)();
    STDMETHOD(WriteLine)(BSTR bstrLine);

    // Errors may be set on this object by other classes.  (Note that
    // these incoming BSTRs can just be wchar_t*'s.)
    STDMETHOD(SetError)(BSTR errorString,
                        BSTR errorURL,
                        BSTR errorHTTPCode);
    STDMETHOD(ClearError());
    
  private:
    HRESULT EnsureXMLDocumentObject(bool bAcquireStream);
    HRESULT EnsureAspServerObject();
    HRESULT WriteToXML(BSTR bstrLine, bool bAddCR);
    HRESULT WriteIdentityXML(asp::IResponse *pResponse);
    HRESULT LoadMasterConfig(CComBSTR & bstrSpecialPIAttrib);
    HRESULT GetServerConfig(IXMLDOMDocument **pServerConfig);
    HRESULT GetDoctype();
    HRESULT InitializeBrowserCapAndAttribs();
    HRESULT ExtractStylesheets(IXMLDOMDocument  *pServerConfig,
                               CComBSTR          arrStylesheets[],
                               short            *pNumStylesheets);
    HRESULT PullStylesheetsFromDeviceInfo(IXMLDOMNode  *pServerConfig,
                                          CComBSTR      arrStylesheets[],
                                          short        *pNumStylesheets);
    HRESULT ApplyStylesheets(asp::IResponse *pResponse,
                             CComBSTR        arrStylesheets[],
                             short           numStylesheets);
    HRESULT LoadXMLFromRelativeLoc(BSTR localName,
                                   BSTR pathName,
                                   bool isConfigXML,
                                   IXMLDOMDocument **ppXMLDoc,
                                   IXSLTemplate    **ppXSLTemplate);
    HRESULT VerifyEncodingAndCharset(UINT *puiCP);
    
  private:
    CComBSTR                        m_bstrURL;
    CComBSTR                        m_bstrURLServerConfig;
    CComBSTR                        m_bstrURLDirectory;
    CComBSTR                        m_bstrConfigDirectory;
    CComBSTR                        m_bstrContentType;
    CComBSTR                        m_bstrDoctypeName;
    CComBSTR                        m_bstrEncoding;
    CComBSTR                        m_bstrCharset;
    CComBSTR                        m_bstrErrorDescrip;
    CComBSTR                        m_bstrErrorURL;
    CComBSTR                        m_bstrErrorHTTPCode;
    CComBSTR                        m_bstrUserAgent;
    CComPtr<IXMLDOMDocument>        m_pcomXMLDocument;
    CComPtr<IStream>                m_pcomXMLDocumentStream;
    CComPtr<asp::IServer>           m_pcomASPServer;
    CComPtr<IDispatch>              m_pcomBrowserTypeDisp;
    PIParseInfo                     m_piParseInfo;
    bool                            m_bInErrorHandling;
    bool                            m_bResponseEndCalled;
};
