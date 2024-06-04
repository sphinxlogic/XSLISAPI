// ============================================================================
// FILE: Global.cpp
//
//      Global definitions, declarations, etc.
//
// Copyright (c) 1999-2000 Microsoft Corporation. All rights reserved.

#include "StdAfx.h"

BSTR              g_bstrServer = NULL;
BSTR              g_bstrBrowserType = NULL;
fso::IFileSystem *g_fileSystemObject = NULL;
CXmlCache        *g_xmlCache = NULL;
Xml3Availability  g_xml3Availability = xml3AvailabilityUnchecked;
bool              g_globallyInitialized = false;

// ============================================================================
// ModuleGlobalInitialize
//      Initializes globals.
//      Returns true if succeeded, false otherwise. The DLL should not
//      be loaded if this call fails.

bool
ModuleGlobalInitialize()
{
    HRESULT hr = S_OK;

    if (g_globallyInitialized) {
        RETURNERR(S_OK);
    }
    
    // Initialize global BSTRs.
    ERRCHECK (
        (g_bstrServer = SysAllocString (L"Server")) == NULL, 
        E_OUTOFMEMORY);
    ERRCHECK (
        (g_bstrBrowserType = SysAllocString (L"MSWC.BrowserType")) == NULL, 
        E_OUTOFMEMORY);

    hr = CoCreateInstance(fso::CLSID_FileSystemObject,
                          NULL,
                          CLSCTX_ALL,
                          fso::IID_IFileSystem,
                          reinterpret_cast<void **>(&g_fileSystemObject));
    HRCHECK(FAILED(hr));

    g_xmlCache = new CXmlCache(60);
    ERRCHECK(g_xmlCache == NULL, E_OUTOFMEMORY);

    g_globallyInitialized = true;

    hr = S_OK;
    
  Error:
    if (FAILED(hr)) {
        ModuleGlobalUninitialize ();
    }
    return SUCCEEDED(hr);
}

// ============================================================================
// ModuleGlobalUninitialize
//      Uninitializes globals.

void
ModuleGlobalUninitialize()
{
    if (g_globallyInitialized) {
        delete g_xmlCache;
        SysFreeString (g_bstrServer);
        SysFreeString (g_bstrBrowserType);
        SAFERELEASE(g_fileSystemObject);
        g_globallyInitialized = false;
    }
}
