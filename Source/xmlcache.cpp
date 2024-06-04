//+---------------------------------------------------------------------------
//
//  Copyright (C) Microsoft Corporation, 1999-2000.
//
//  File:       xmlcache.cpp
//
//  Contents:   Implementation of CXmlCache which is a cache of XML documents 
//              that has a Least Recently Used cleanup algorithm that cleans 
//              up things that have not been used for a given number of minutes.
//----------------------------------------------------------------------------
#include "StdAfx.h"
#include "xmlcache.h"

/////////////////////////////////////////
// CXmlCacheEntry
/////////////////////////////////////////

class CXmlCacheEntry : public IUnknown
{
  public:
    CXmlCacheEntry() {
        m_ref = 1;
        m_pUnk = NULL;
        ::memset(&m_ftLastWrite, 0, sizeof(FILETIME));
        m_nFileSize = 0;
        m_lastUsed = ::GetTickCount();
    }
    
    CXmlCacheEntry::~CXmlCacheEntry() {
        SAFERELEASE(m_pUnk);
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface( 
        /* [in] */ REFIID /*riid*/,
        /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR * /*ppvObject*/) {
        return E_NOTIMPL;
    }
    
    virtual ULONG STDMETHODCALLTYPE AddRef(void) {
        return ++m_ref;
    }
    
    virtual ULONG STDMETHODCALLTYPE Release(void) {
        long result = --m_ref;
        if (m_ref == 0) 
            delete this;
        return result;
    }

  private:
    friend class CXmlCache;

    long                    m_ref;
    IUnknown               *m_pUnk;
    FILETIME                m_ftLastWrite;
    DWORD                   m_nFileSize;
    DWORD                   m_lastUsed;
};

/////////////////////////////////////////
// CXmlCache
/////////////////////////////////////////

CXmlCache::CXmlCache(long minutes) 
{
    InitializeCriticalSection(&m_cs);
    SetMinutes(minutes);
    m_table.init(17,0.8,1.5);
    m_lastCleanup = ::GetTickCount();
    m_lastRecordedTime = ::GetTickCount();

}

CXmlCache::~CXmlCache()
{
    DeleteCriticalSection(&m_cs);
}

HRESULT
CXmlCache::SetMinutes(long minutes)
{
    Enter();
    if (minutes == 0) {
        m_bCacheDisabled = true;
    } else {
        m_bCacheDisabled = false;
        m_ticksBeforeDispose = minutes * 60 * 1000;
    }
    Leave();
    return S_OK;
}

// CXmlCache::Lookup
//     Lookup XML file in cache.  Be sure it's up-to-date.  If not, or 
//     nonexistent, read from file.  Outgoing pointer is addref'd.

// Unfortunately, the use of the variable 'data' below triggers
// warning 4701, saying variable may be used w/o being initialized.
// That's not possible given the logic of the code, but the compiler
// doesn't know it -- so we disable the warning for this function. 
#pragma warning(disable:4701)

HRESULT
CXmlCache::Lookup(char *pszStylesheet,                 // [in] full path to local file
                  wchar_t *pwszURL,                    // [in] user-meaningful URL
                  bool     bIsHTTPPath,                // [in] whether this is an http:// path
                  CXMLServerDocument *pRequester,      // [in] request server object

                  // Only one of these two will be filled in.  If
                  // ppTemplateResult is non-NULL, we'll QI for an
                  // IXSLTemplate.  If that fails, or ppTemplateResult
                  // is NULL, we'll QI for the IXMLDOMDocument.
                  IXMLDOMDocument **ppDOMResult,        // [out] result, AddRef'd
                  IXSLTemplate    **ppTemplateResult)   // [out] result, AddRef'd
{
    HRESULT                         hr = S_OK;
    CComPtr<IUnknown>               pcomNewUnk;
    WIN32_FIND_DATAA                data;
    CXmlCacheEntry                 *entry = NULL;

    bool bDoNotUseCache = bIsHTTPPath || m_bCacheDisabled;

    *ppDOMResult = NULL;
    if (ppTemplateResult) {
        *ppTemplateResult = NULL;
    }

    if (!bDoNotUseCache) {

        // Only use the cache on local files, since we don't know
        // whether http:// based files are expired, and we don't deal
        // with HTTP expiration/timeout stuff here.
        CleanupCache();

        Enter(); // lock table for lookup
        entry = (CXmlCacheEntry*)m_table.find(pszStylesheet);
        Leave();

        HANDLE h = FindFirstFileA(pszStylesheet, &data);
        bool filefound = (h != INVALID_HANDLE_VALUE);
        FindClose(h);

        if (!filefound) {
            // Even if it's in the cache, say it's not there, since it's
            // no longer on disk.
            pRequester->SetError(L"Resource not found",
                                 pwszURL,
                                 L"404 Not Found");
            RETURNERR(E_FAIL);
        }
    
        if (entry) {

            // don't ping the disk more than once every 2 seconds.  In the
            // event of tick count wraparound (~ every 50 days), we may
            // ping more than once every 2 seconds.
            DWORD t = ::GetTickCount();
            if (entry->m_lastUsed + 2000 >= t) {
                // use what we have !
                pcomNewUnk = entry->m_pUnk; // addref's, but will be released on destruction
                RETURNERR(S_OK);  
            }

            entry->m_lastUsed = ::GetTickCount(); // update last used field.

            if (memcmp(&entry->m_ftLastWrite,
                       &data.ftLastWriteTime,
                       sizeof(FILETIME)) == 0 &&
                entry->m_nFileSize == data.nFileSizeLow) {

                // use what we have !
                pcomNewUnk = entry->m_pUnk; // addref's, but will be released on destruction
                RETURNERR(S_OK);
            }

            // cache is out of date, we are going to reload the pszStylesheet.
        }

    }

    // Need a new object (so we don't clobber the existing
    // document in case it is still being used)
    {
        CComPtr<IXMLDOMDocument> pcomNewXML;
        
        hr = CreateXMLDocumentOnCComPtr(pcomNewXML);
        HRCHECK(FAILED(hr));

        hr = ReallyLoadXMLDocument(pcomNewXML,
                                   CComBSTR(pszStylesheet),
                                   pwszURL,
                                   bIsHTTPPath,
                                   pRequester);
        HRCHECK(FAILED(hr));

        // Assume we'll store the XML unless we figure out otherwise. 
        pcomNewUnk = pcomNewXML;
        if (ppTemplateResult) {
            CComPtr<IXSLTemplate> pcomTemplate;
            hr = pcomTemplate.CoCreateInstance(CLSID_XSLTemplate);
            if (SUCCEEDED(hr)) {
                // TODO: Perhaps useful error information is provided here  
                // when there's an error in the stylesheet.
                hr = pcomTemplate->putref_stylesheet(pcomNewXML);

                if (FAILED(hr)) {
                    pRequester->SetErrorToLastCOMError(pwszURL);
                    RETURNERR(hr);
                }
                
                pcomNewUnk = pcomTemplate;
            }
            hr = S_OK;
        }
    }

    if (!bDoNotUseCache) {
        
        if (! entry) {
            entry = new CXmlCacheEntry();
            ERRCHECK(entry == NULL, E_OUTOFMEMORY);

            entry->m_lastUsed = ::GetTickCount();
            entry->m_pUnk = pcomNewUnk;
            entry->m_pUnk->AddRef(); // addref for the hashtable entry
        
            // Need to lock the table while we update it.
            Enter();
            m_table.add(pszStylesheet, entry);
            Leave();
        
        } else {
        
            // Now we are finished with old object.  Put the new one in, and
            // release the old one.
            IUnknown *pOldUnk =
                reinterpret_cast<IUnknown *>(InterlockedExchange(
                    reinterpret_cast<long*>(&entry->m_pUnk),
                    reinterpret_cast<long>(pcomNewUnk.p)));
            SAFERELEASE(pOldUnk);
            entry->m_pUnk->AddRef(); // addref for the hashtable entry
        }

        entry->m_ftLastWrite = data.ftLastWriteTime;
        entry->m_nFileSize = data.nFileSizeLow;

    }

    hr = S_OK;
  Error:
    if (hr == S_OK) {

        ASSERT(g_xml3Availability != xml3AvailabilityUnchecked);
        
        if (ppTemplateResult &&
            g_xml3Availability == xml3AvailabilityAvailable) {
            hr = pcomNewUnk->QueryInterface(IID_IXSLTemplate,
                                            reinterpret_cast<void**>(ppTemplateResult));
        }
        
        if (!ppTemplateResult ||
            g_xml3Availability == xml3AvailabilityUnavailable ||
            FAILED(hr)) {
            
            hr = pcomNewUnk->QueryInterface(IID_IXMLDOMDocument,
                                            reinterpret_cast<void**>(ppDOMResult));
            HRCHECK(FAILED(hr));
            
        }
    }
    SAFERELEASE(entry);
    return hr;
}
#pragma warning(default:4701)

void 
CXmlCache::ClearCache()
{
    // Need to lock the table while we update it.
    Enter();
    m_table.clear();
    Leave();
}

void
CXmlCache::CleanupCache()
{
    DWORD now = ::GetTickCount();
    DWORD lastTime = m_lastRecordedTime;
    m_lastRecordedTime = now;    
    if (now < lastTime) {
        // We've wrapped around (happens after ~50 days).  Clear out
        // the cache, since lastUsed times are now meaningless.
        this->ClearCache();
    }
    
    // If we haven't done a cleanup since m_ticksBeforeDispose ago
    // then do one and throw away any stylesheets that haven't been 
    // used since m_ticksBeforeDispose ago.  For example, every hour, 
    // throw away all stylesheets that weren't used
    // during that hour.
    if ((now - m_lastCleanup) > m_ticksBeforeDispose)
    {
        // Need to lock the table while we update it.
        Enter();

        // time to do a cleanup sweep.
        long size = m_table.getCapacity();
        for (long i = 0; i < size; i++)
        {
            HashEntry* e = m_table.get(i);
            // walk the bucket list.
            while (e)
            {
                HashEntry* next = e->m_pNext;
                CXmlCacheEntry* cache = (CXmlCacheEntry*)e->m_pUnk;
                if ((now - cache->m_lastUsed) < m_ticksBeforeDispose)
                {
                    m_table.remove(e->m_szKey);
                }                
                e = next;
            }
        }
        m_lastCleanup = now;

        Leave();
    }

}


