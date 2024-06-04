// ============================================================================
// FILE: AxpPreprocessor.cpp
//
//      Implementation of ASP Preprocessor object. (see ASPPreprocessor.h
//      for details)
//
//      NOTES:
//      The ASP preprocessor object is intended to be used as a single
//      instance object with application scope. The object is "both
//      threaded", allowing access from any thread.
//
//      THREADING AND THE REQUEST TABLE:
//      Because multiple threads are capable of calling into the object
//      at the same time, possibly requesting the same file, the 
//      preprocessor needs to implement threading support. It does 
//      this through a request table.
//  
//      The request table contains a list of all files currently being
//      processed. This includes files that are currently running
//      through the preprocessor, as well as files that are being checked
//      for the need to do preprocessing. There is a limit of how many
//      files can be in the request table at one time.
//
//      When a client calls Process for a file, the thread tries to 
//      acquire a request for the file from the request table. 
//
//      When a thread tries to acquire a request for a file that 
//      does not exist in the request table, the object tries to create
//      a new request. If the request table is full, the thread blocks
//      until a slot becomes empty.
//
//      If the request has been newly created, this thread is then
//      responsible for processing the request, and marking it as done.
//      If the request was created earlier, this thread simply waits
//      for the request to complete. Once done, the thread releases
//      the acquired request.
//
//      When a request is released by all threads that acquired it, it
//      is removed from the request table. If there are threads waiting
//      on a full request table, one of these threads are then released.
//
//      The request table is implemented as a double array. The array
//      of Request structures contain information about each request.
//      The second array contains indices into the first array,
//      sorted alphabetically. This allows the search code to efficiently
//      look up a filename. Indices beyond the current size of the
//      request table point to empty slots in the first array. This
//      allows quick search for an empty request slot.
//
// Copyright (c) 1999-2000 Microsoft Corporation. All rights reserved.

#include "StdAfx.h"
#include "ASPPreprocessor.h"
#include "PreProcessor.h"

// ============================================================================
// CLASS: CUserSecurityMode
//      Class encapsulating client impersonation. If you are running
//      in an impersonated thread, you can create this object and call
//      Capture to get the current impersonation. Then, you can use
//      Enter and Exit to make a thread enter and exit this same 
//      impersonation.
                  
class CUserSecurityMode
{
public:
    CUserSecurityMode()
        { m_hToken = NULL; }
    ~CUserSecurityMode()
        { 
            if (m_hToken != NULL) 
            {
                CloseHandle (m_hToken);
            }
        }

    // Captures the current impersonation. Returns boolean indicating
    // success. 
    bool Capture() {
        ASSERT (m_hToken == NULL);
        return OpenThreadToken (GetCurrentThread (), 
                                TOKEN_QUERY | TOKEN_IMPERSONATE, 
                                TRUE, 
                                &m_hToken) != FALSE;
    }
    

    // Enters impersonation. The thread on which you call capture will
    // initially be in impersonation mode. Returns boolean indicating
    // success. 
    bool Enter() {
        ASSERT (m_hToken != NULL);
        return ImpersonateLoggedOnUser(m_hToken) != FALSE;
    }

    // Turns off impersonation. The thread on which you call capture
    // will initially be in impersonation mode.  Returns boolean
    // indicating success. 
    bool Exit() {
        return RevertToSelf() != FALSE;
    }


private:
    HANDLE m_hToken;                        // User security token
};

// ============================================================================
// CLASS: CWin32FileWriter
//     Subclasses Writer (declared in PreProcessor.h) and implements
//     OutputWrite for our usage of PreProcess. 

class CWin32FileWriter : public Writer 
{
public:
    CWin32FileWriter(HANDLE hfile) : m_hfile(hfile) {}
    virtual HRESULT Out(LPCSTR psz, int nLength);

private:
    HANDLE m_hfile;
};

// ============================================================================
// CASPPreprocessor::CASPPreprocessor
//      Class constructor.

CASPPreprocessor::CASPPreprocessor()
{
    m_pUnkMarshaler = NULL;
    m_nRequests = 0;
    m_heventSlotFree = NULL;

    // Initialize the request table.
    for (int i = 0; i < g_nMaxConcurrentPreprocessorRequests; i++) {
        m_nRequestSlots[i] = i;
    }

    // Use a spin-count critical section to lock access
    // to the request table.
    InitializeCriticalSectionAndSpinCount (&m_critsecLock, 4000);
}

// ============================================================================
// CASPPreprocessor::~CASPPreprocessor
//      Class destructor.

CASPPreprocessor::~CASPPreprocessor()
{
    DeleteCriticalSection (&m_critsecLock);
    if (m_heventSlotFree != NULL)
    {
        CloseHandle (m_heventSlotFree);
    }
}

// ============================================================================
// CASPPreprocessor::FinalConstruct
//      Called to complete construction of the object. 
//      Returns HRESULT indicating success.

HRESULT 
CASPPreprocessor::FinalConstruct()
{
    // Create the event that will be used to signal threads
    // waiting on a full request table.

    m_heventSlotFree = CreateEvent (NULL, FALSE, FALSE, NULL);
    if (m_heventSlotFree == NULL)
    {
        return E_OUTOFMEMORY;
    }

    return CoCreateFreeThreadedMarshaler (
        GetControllingUnknown (), &m_pUnkMarshaler.p);
}

// ============================================================================
// CASPPreprocessor::FinalRelease
//      Called to complete end of the object. 

void 
CASPPreprocessor::FinalRelease()
{
    m_pUnkMarshaler.Release ();
}

// ============================================================================
// CASPPreprocessor::Process
//      Preprocesses a given file. Actual preprocessing only occurs if
//      the file has not already been preprocessed, or if it has changed
//      since the last processing.
//
//      The preprocessed version of the file is written out to the same
//      directory as the source file, and a server path to it is returned
//      in pbstrOutFile.
//
//      Returns HRESULT indicating success.

#define REQUEST_ID_NOT_YET_SET -1

STDMETHODIMP              
CASPPreprocessor::Process(
    BSTR bstrSrcFile,                       // [in] Source file
    BSTR* pbstrOutFile)                     // [out] Receives target file
{
    HRESULT hr;
    int nSrcFilePathLen;
    bool bNewRequest;
    int nRequestID = REQUEST_ID_NOT_YET_SET;
    BSTR bstrOutFile = NULL;

    const int suffixLen = 4;

    ASSERT_VALID_BSTR (bstrSrcFile);
    ERRCHECK (bstrSrcFile == NULL, E_INVALIDARG);

    // Make sure the string is within limits.
    nSrcFilePathLen = SysStringLen (bstrSrcFile);
    ERRCHECK (nSrcFilePathLen < suffixLen + 1, E_INVALIDARG);
    ERRCHECK (nSrcFilePathLen >= _MAX_PATH,
        HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW));
    ERRCHECK (bstrSrcFile[nSrcFilePathLen - (suffixLen+1)] != L'.', E_INVALIDARG);

    // Create the output file name by adding a __ onto the front of
    // the filename, and changing the suffix to .asp.  Thus,
    //    /virtDir1/subDir1/SubDir2/foo.pasp
    // will become
    //    /virtDir1/subDir1/SubDir2/__foo.asp

    {
        // Shouldn't be any '?' in the URL.  These have been stripped off
        // by the filter.
        ASSERT(wcsstr(bstrSrcFile, L"?") == NULL);

        // Search the path for the last '/' before the end 
        wchar_t *pwszLastSlash = &bstrSrcFile[nSrcFilePathLen-1];
        while (pwszLastSlash >= bstrSrcFile && *pwszLastSlash != L'/') {
            pwszLastSlash--;
        }

        int slashLoc = pwszLastSlash - bstrSrcFile;

        int nNameWithoutExtensionLen =
            nSrcFilePathLen -
            (slashLoc + 1) -
            suffixLen;

        ASSERT(*pwszLastSlash == L'/');      // we assume there's one somewhere

        bstrOutFile = SysAllocStringLen(NULL, nSrcFilePathLen + 1);
        ERRCHECK (bstrOutFile == NULL, E_OUTOFMEMORY);
        memcpy(bstrOutFile,
               bstrSrcFile,
               (slashLoc + 1) * sizeof(wchar_t));
        bstrOutFile[slashLoc + 1] = L'_';
        bstrOutFile[slashLoc + 2] = L'_';
        memcpy(&bstrOutFile[slashLoc + 3],
               &bstrSrcFile[slashLoc + 1],
               nNameWithoutExtensionLen * sizeof(wchar_t));
        bstrOutFile[nSrcFilePathLen + 2 - 4] = L'a';
        bstrOutFile[nSrcFilePathLen + 2 - 3] = L's';
        bstrOutFile[nSrcFilePathLen + 2 - 2] = L'p';
        bstrOutFile[nSrcFilePathLen + 2 - 1] = L'\0';
    }

    // Acquire the request. May fail if it takes too long!
    hr = AcquireRequest (bstrSrcFile, &nRequestID, &bNewRequest);
    HRCHECK (FAILED(hr));

    if (bNewRequest)
    {
        // This is a new request (no one else is already processing
        // it). Process it ourselves.
        hr = ProcessRequest (bstrSrcFile, bstrOutFile);
        m_req[nRequestID].bIsDone = true;
        HRCHECK(FAILED(hr));
    }
    else
    {
        // Wait for the request to be done (by someone else).
        while (!m_req[nRequestID].bIsDone)
        {
            SwitchToThread ();
        }
    }

    *pbstrOutFile = bstrOutFile;

Cleanup:
    // Release the request. Will free it if this is the last thread
    // sitting on it.
    if (nRequestID != REQUEST_ID_NOT_YET_SET) {
        ReleaseRequest (nRequestID);
    }

    return hr;

Error:
    SysFreeString (bstrOutFile);
    *pbstrOutFile = NULL;
    goto Cleanup;
}


// ============================================================================
// CASPPreprocessor::AcquireRequest
//      Acquires a request from the request table for the given file,
//      creating a new request if needed.
//      Returns HRESULT indicating success. May fail on a timeout.

HRESULT 
CASPPreprocessor::AcquireRequest(
    BSTR bstrSource,                        // [in] Source file
    int* pnRequestID,                       // [out] Request ID
    bool* pbNewRequest)                     // [out] Newly created?
{
    ASSERT (bstrSource != NULL);
    ASSERT (pnRequestID != NULL);
    ASSERT (pbNewRequest != NULL);

    *pbNewRequest = true;
    while (true)
    {
        EnterCriticalSection (&m_critsecLock);

        // Do a binary search to look up the filename.

        int nLeft, nRight, nPivot, nCmp;
        nPivot = -1;
        nLeft = 0;
        nRight = m_nRequests - 1;
        
        while (nLeft <= nRight)
        {
            nPivot = (nLeft + nRight) / 2;
            nCmp = lstrcmp (bstrSource, m_req[m_nRequestSlots[nPivot]].wszFileName);
            if (nCmp == 0)
            {
                *pbNewRequest = false;
                break;
            }
            else if (nCmp < 0)
            {
                nRight = nPivot - 1;
            }
            else
            {
                nLeft = nPivot + 1;
            }
        }
    
        if (*pbNewRequest)
        {
            if (m_nRequests < g_nMaxConcurrentPreprocessorRequests)
            {
                // There's a slot available in the request table. Use it.

                                int nNewPos = (nLeft > nPivot) ? nPivot + 1 : nPivot;
                int nFreeSlot = m_nRequestSlots[m_nRequests];

                // Shift indices to preserve sorted state.

                for (int i = m_nRequests; i > nNewPos; i--)
                {
                    m_nRequestSlots[i] = m_nRequestSlots[i - 1];
                }
                m_nRequestSlots[i] = nFreeSlot;

                lstrcpy (m_req[nFreeSlot].wszFileName, bstrSource);
                m_req[nFreeSlot].lUseCount = 1;
                m_req[nFreeSlot].bIsDone = false;

                m_nRequests++;
                *pnRequestID = nFreeSlot;
                break;
            }
            else
            {
                // Table is full! Wait for someone to signal that it has
                // a slot free again.

                LeaveCriticalSection (&m_critsecLock);
                if (WaitForSingleObject (m_heventSlotFree, 5000) == WAIT_TIMEOUT)
                {
                    return HRESULT_FROM_WIN32(ERROR_SEM_TIMEOUT);
                }
            }
        }
        else
        {
            // A request already exists for this file.

            *pnRequestID = m_nRequestSlots[nPivot];
            m_req[*pnRequestID].lUseCount++;
            break;
        }
    }

    LeaveCriticalSection (&m_critsecLock);
    return S_OK;
}

// ============================================================================
// CASPPreprocessor::ReleaseRequest
//      Releases a request. If all references for a request are
//      released, the request is removed from the table.

void 
CASPPreprocessor::ReleaseRequest(
    int nRequestID)                         // [in] Request ID
{
    ASSERT (nRequestID >= 0 && 
        nRequestID < g_nMaxConcurrentPreprocessorRequests);

    EnterCriticalSection (&m_critsecLock);
    if (--(m_req[nRequestID].lUseCount) == 0)
    {
        // No more references, so remove the request.

        int nSlot, i;

        // Find this request in the index array.

        for (nSlot = m_nRequests - 1; 
             m_nRequestSlots[nSlot] != nRequestID; 
             nSlot--)
        {
            ASSERT (nSlot >= 0);
        }

        // Shift indices to preserve sorted state.

        for (i = m_nRequests - 1; i > nSlot; i--)
        {
            m_nRequestSlots[i - 1] = m_nRequestSlots[i];
        }

        m_nRequests--;

        // If the table was previously full, set an event to allow the
        // next thread waiting on this to get a chance.

        if (m_nRequests == g_nMaxConcurrentPreprocessorRequests - 1)
        {
            SetEvent (m_heventSlotFree);
        }
    }
    LeaveCriticalSection (&m_critsecLock);
}

// ============================================================================
// CASPPreprocessor::ProcessRequest
//      Processes a request for a single file. Only one thread can
//      process a request for any given file at one time.
//
//      Returns HRESULT indicating success.

HRESULT
CASPPreprocessor::ProcessRequest(
    BSTR bstrSource,
    BSTR bstrTarget)
{
    HRESULT hr;
    CComBSTR bstrSourceLocalPath;
    CComBSTR bstrTargetLocalPath;
    bool bIsStale;

    // Get the local pathname for the files.
    hr = GetLocalPath (bstrSource, &bstrSourceLocalPath);
    HRCHECK (FAILED(hr));

    hr = GetLocalPath (bstrTarget, &bstrTargetLocalPath);
    HRCHECK (FAILED(hr));

    hr = IsFileStale (bstrSourceLocalPath, bstrTargetLocalPath, &bIsStale);
    HRCHECK (FAILED(hr));

    if (bIsStale)
    {
        hr = RunPreprocessor (bstrSourceLocalPath, bstrTargetLocalPath);
        HRCHECK (FAILED(hr));
    }

    hr = S_OK;
  Error:
    return hr;
}

// ============================================================================
// CASPPreprocessor::GetLocalPath
//      Uses Server object to get the local path for a given relative
//      path.
//      Returns HRESULT indicating success.

HRESULT 
CASPPreprocessor::GetLocalPath(
    BSTR bstrPath,                          // [in] Filename
    BSTR* pbstrLocalPath)                   // [out] Receives local path
{
    CComPtr<IServer> pcomServer;

    ASSERT (bstrPath != NULL);
    ASSERT (pbstrLocalPath != NULL);

    HRESULT hr;

    hr = GetASPServerObject (&pcomServer);
    HRCHECK (FAILED(hr));

    hr = pcomServer->MapPath (bstrPath, pbstrLocalPath);
    HRCHECK (FAILED(hr));
    ERRCHECK (*pbstrLocalPath == NULL, E_FAIL);

Cleanup:
    return hr;

Error:
    goto Cleanup;
}

// ============================================================================
// CASPPreprocessor::IsFileStale
//      Checks if the target file doesn't exist or is older than the
//      source file.
//      Returns HRESULT indicating success.

HRESULT 
CASPPreprocessor::IsFileStale(
    LPCWSTR pwszSourceFile,                 // [in] Source path
    LPCWSTR pwszTargetFile,                 // [in] Source path
    bool* pbIsStale)                        // [out] Test result
{
    HRESULT hr = S_OK;
    WIN32_FILE_ATTRIBUTE_DATA wfadSourceFile;
    WIN32_FILE_ATTRIBUTE_DATA wfadTargetFile;
    BOOL b;

    // Use GetFileAttributesEx function, which is supposedly pretty
    // fast, especially in NT 5.0. This avoids actually opening the
    // files unless we need to process them.

    b = GetFileAttributesEx (pwszTargetFile, GetFileExInfoStandard, 
            &wfadTargetFile);
    if (!b)
    {
        *pbIsStale = true;
        goto Cleanup;
    }

    // Error if the target file is read-only or a directory.
    ERRCHECK ((wfadTargetFile.dwFileAttributes & 
                (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_DIRECTORY)) != 0,
                E_UNEXPECTED);

    b = GetFileAttributesEx (pwszSourceFile, GetFileExInfoStandard, 
            &wfadSourceFile);
    ERRCHECK (!b, HRESULT_FROM_WIN32(GetLastError ()));

    // Error if the source file is a directory.
    ERRCHECK ((wfadSourceFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0,
        E_UNEXPECTED);

    *pbIsStale = CompareFileTime (&wfadSourceFile.ftLastWriteTime,
                    &wfadTargetFile.ftLastWriteTime) > 0;

Cleanup:
    return hr;

Error:
    goto Cleanup;
}

// ============================================================================
// CASPPreprocessor::RunPreprocessor
//      Creates a target file, loads the source file contents, 
//      and runs the preprocessor, generating processed content in the
//      target file.
//      Returns HRESULT indicating success.

HRESULT 
CASPPreprocessor::RunPreprocessor(
    LPCWSTR pwszSourceFile,                 // [in] Source path
    LPCWSTR pwszTargetFile)                 // [out] Target path
{
    HRESULT hr = S_OK;
    HANDLE hfileSource = INVALID_HANDLE_VALUE;
    HANDLE hfileTarget = INVALID_HANDLE_VALUE;
    LPSTR pszAllocatedData = NULL;
    CUserSecurityMode usm;
    bool bEnteredSecurityContext = false;

    // We actually need to create a file and write to it. This is not
    // possible with the current permission level, which is usually
    // that of a guest or anonymous user. So we need to temporarily
    // "unimpersonate" the user, and go into our own self.

    ERRCHECK (!usm.Capture (), HRESULT_FROM_WIN32 (GetLastError ()));
    ERRCHECK (!usm.Exit (), HRESULT_FROM_WIN32 (GetLastError ()));
    bEnteredSecurityContext = true;
      
    // Open the source file.

    hfileSource = CreateFile (pwszSourceFile,
                    GENERIC_READ,
                    FILE_SHARE_READ,
                    NULL,
                    OPEN_EXISTING,
                    FILE_FLAG_SEQUENTIAL_SCAN,
                    NULL);
    ERRCHECK (hfileSource == INVALID_HANDLE_VALUE, 
        HRESULT_FROM_WIN32 (GetLastError ()));

    // Create the target file, deleting one if it exists.

    hfileTarget = CreateFile (pwszTargetFile,
                    GENERIC_WRITE,
                    0,
                    NULL,
                    CREATE_ALWAYS,
                    0,
                    NULL);
    ERRCHECK (hfileTarget == INVALID_HANDLE_VALUE, 
        HRESULT_FROM_WIN32 (GetLastError ()));

    {
        DWORD dwSourceSize;
        dwSourceSize = GetFileSize (hfileSource, NULL);
        ERRCHECK (dwSourceSize == (DWORD)-1L, 
            HRESULT_FROM_WIN32 (GetLastError ()));

        // For smaller files, we can just use memory on the
        // stack to hold the file. For bigger files, we go with
        // an allocated buffer.

        LPSTR pszData;
        if (dwSourceSize < 4096)
        {
            pszData = (LPSTR)_alloca (dwSourceSize + 1);
        }
        else
        {
            pszData = new char[dwSourceSize + 1];
            ERRCHECK (pszData == NULL, E_OUTOFMEMORY);
            pszAllocatedData = pszData;
        }

        // Read the file into memory, creating a nul-terminated
        // buffer for it, as the preprocessor expects.

        DWORD dwReadSize;
        
        BOOL b = ReadFile (hfileSource, 
                    pszData, 
                    dwSourceSize, 
                    &dwReadSize, 
                    NULL);

        ERRCHECK (!b, HRESULT_FROM_WIN32 (GetLastError ()));
        ERRCHECK (dwReadSize < dwSourceSize, E_UNEXPECTED);
        pszData[dwSourceSize] = L'\0';

        CWin32FileWriter fw (hfileTarget);

        hr = ScanAndPreprocess (pszData, dwSourceSize, fw);
        HRCHECK (FAILED(hr));
    }

Cleanup:

    if (pszAllocatedData == NULL)
    {
        delete[] pszAllocatedData;
    }
    if (hfileSource != INVALID_HANDLE_VALUE)
    {
        CloseHandle (hfileSource);
    }
    if (hfileTarget != INVALID_HANDLE_VALUE)
    {
        CloseHandle (hfileTarget);
    }
    if (bEnteredSecurityContext)
    {
        usm.Enter ();
    }

    return hr;

Error:

    goto Cleanup;
}

// ============================================================================
// CWin32FileWrite::Out
//      Overriden virtual function, to support output from the preprocessor.
//      Just writes to the file handle passed during construction.
//      I don't think we need any buffering, or any asynchronous writes,
//      for now.
//      Returns HRESULT indicating success.

HRESULT 
CWin32FileWriter::Out(
    LPCSTR psz,                             // [in]string to write out
    int nLength)                            // [in]num chars to write 
{
    HRESULT hr = S_OK;

    if (nLength > 0)
    {
        ASSERT (psz != NULL);
        DWORD dwWritten;
        
        BOOL bRet = WriteFile (m_hfile, psz, nLength, &dwWritten, NULL);

        ERRCHECK (!bRet, HRESULT_FROM_WIN32 (GetLastError ()));
        ERRCHECK (static_cast<DWORD>(nLength) != dwWritten, E_UNEXPECTED);
    }

Cleanup:

    return hr;

Error:

    goto Cleanup;

}
