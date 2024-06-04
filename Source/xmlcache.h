//+---------------------------------------------------------------------------
//
//  Copyright (C) Microsoft Corporation, 1999-2000
//
//  File:       xmlcache.h
//
//  Contents:   Defines CXmlCache which is a cache of XML documents 
//              that has a Least Recently Used cleanup algorithm that cleans 
//              up things that have not been used for a given number of minutes.
//----------------------------------------------------------------------------

#pragma once

#include "hashtable.h"

class CXmlCache
{
  public:
    CXmlCache(long minutes);
    ~CXmlCache();

    HRESULT SetMinutes(long minutes);

    // Lookup XML file in cache.  Be sure it's up-to-date.  If not, or
    // nonexistent, read from file.  Outgoing pointer is addref'd.
    HRESULT Lookup(char *stylesheet,                    // [in] full path to local file
                   wchar_t *pwszURL,                    // [in] user-meaningful URL
                   bool bIsHTTPPath,                    // [in] whether this is a http:// path
                   CXMLServerDocument *pRequester,      // [in] request server object

                   // One of the next two will be output, the other
                   // NULL, depending on what's in the cache.  If the
                   // incoming ppTemplateResult is not NULL, we'll try
                   // to create a IXSLTemplate out of the document.
                   IXMLDOMDocument **ppDOMResult,     // [out] result, AddRef'd
                   IXSLTemplate    **ppTemplateResult // [out] result, AddRef'd
                   );  

  private:
    void ClearCache();
    void CleanupCache();
    void Enter() {
        EnterCriticalSection(&m_cs);
    }

    void Leave() {
        LeaveCriticalSection(&m_cs);
    }

    HashTable        m_table;
    DWORD            m_ticksBeforeDispose;
    DWORD            m_lastCleanup;
    DWORD            m_lastRecordedTime;
    bool             m_bCacheDisabled;
    CRITICAL_SECTION m_cs; // need to lock cache on updates.
};

