// ============================================================================
// FILE: ASPPreprocessor.h
//
//      Definition of ASP Preprocessor object.
//
//      The ASP Preprocessor object reads ASP files (ASP files that
//      declaratively generate XML), and modifies them so that they
//      create and write into an XMLServerDocument object instead, and
//      then produce output by calling the XMLServerDocument's Transform
//      method.
//
// Copyright (c) 1999-2000 Microsoft Corporation. All rights reserved.


#pragma once

#include "resource.h"       // main symbols

// Number of maximum concurrent requests.

const int g_nMaxConcurrentPreprocessorRequests = 64;

using namespace asp;

// ============================================================================
// CLASS: CASPPreprocessor
//      Class that implements ASPPreprocessor object.

class ATL_NO_VTABLE CASPPreprocessor : 
        public CComObjectRootEx<CComMultiThreadModel>,
        public CComCoClass<CASPPreprocessor, &CLSID_ASPPreprocessor>,
        public IDispatchImpl<IASPPreprocessor, &IID_IASPPreprocessor, &LIBID_XSLISAPI2Lib>
{
  public:
    CASPPreprocessor();
    ~CASPPreprocessor();

    DECLARE_REGISTRY_RESOURCEID(IDR_ASPPREPROCESSOR)
    DECLARE_NOT_AGGREGATABLE(CASPPreprocessor)
    DECLARE_GET_CONTROLLING_UNKNOWN()
    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(CASPPreprocessor)
        COM_INTERFACE_ENTRY(IASPPreprocessor)
        COM_INTERFACE_ENTRY(IDispatch)
        COM_INTERFACE_ENTRY_AGGREGATE(IID_IMarshal, m_pUnkMarshaler.p)
    END_COM_MAP()

    HRESULT FinalConstruct();
    void FinalRelease();
    CComPtr<IUnknown> m_pUnkMarshaler;

    // IASPPreprocessor
  public:
    STDMETHOD(Process)(BSTR bstrSrcFile, BSTR* pbstrOutFile);

  protected:
    // Request structure, used to hold information about each request
    // being processed.

    struct Request
    {
        WCHAR wszFileName[_MAX_PATH];
        long lUseCount;
        bool bIsDone;
    };

    HRESULT GetLocalPath(BSTR bstrPath, BSTR* pbstrLocalPath);
    HRESULT IsFileStale(LPCWSTR pwszSourceFile, 
                LPCWSTR pwszTargetFile, bool* pbIsStale);
    HRESULT RunPreprocessor(LPCWSTR pwszSourceFile, LPCWSTR pwszTargetFile);
    HRESULT AcquireRequest(BSTR bstrSource, 
                int* pnRequestID, bool* pbNewRequest);
    void ReleaseRequest(int nRequestID);
    HRESULT ProcessRequest(BSTR bstrSourceFile, BSTR bstrTargetFile);

    // Request table. For details, see ASPPreprocessor.cpp
    int m_nRequests;                        // No. of requests being handled
    Request m_req[g_nMaxConcurrentPreprocessorRequests];
                                            // Request array
    int m_nRequestSlots[g_nMaxConcurrentPreprocessorRequests];
                                            // Indices into m_req
    CRITICAL_SECTION m_critsecLock;         // Critsec locking request table
    HANDLE m_heventSlotFree;                // Event - slot has freed up
};

