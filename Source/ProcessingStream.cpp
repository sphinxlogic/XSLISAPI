// ============================================================================
// FILE: ProcessingStream.cpp
//
//        Implements a stream intermediary to allow additional processing.
//
// Copyright (c) 1999-2000 Microsoft Corporation. All rights reserved.

#include "StdAfx.h"
#include "charset.h"

// ============================================================================
// NOTES ON HOW THE POSTPROCESSOR WORKS
//
// The postprocessor works in a stream. The caller writes one or more times
// to the postprocessor, with chunks of data, and the preprocessor writes
// chunks of processed data to the output stream. Because of the way
// preprocessing works, each call to ProcessorWrite does not necessarily
// write all of the data passed in the call. At the end, the caller must
// call ProcessorFlush to flush any remaining data.
//
// The postprocessor can currently perform the following tasks:
//      (1) expand XML /> notation for empty tags
//      (2) remove certain closing tags
//      (3) replace certain entity references
//      (4) replace certain characters
//
// The postprocessor has very limited lookahead/lookback capabilities.
// Because of this restriction, the following additional restrictions apply:
//      - any tag name, entity name or string to match must be 32 
//        characters or less. (defined by g_nMaxResidualLength)
//      - characters in (4) cannot include < or &

   
typedef LPCWSTR CLOSING_TAG;

// ============================================================================
// STRUCT: ENTITY_REPLACEMENT
//      Defines a single entity replacement. The postprocessor can accept
//      arrays of these, sorted by name, and map all matched entities
//      to their corresponding output strings. 
                      
struct ENTITY_REPLACEMENT
{
    LPCWSTR pwszName;                       // Entity to match (without the & and ;)
    LPCWSTR pwszReplace;                    // Text to replace with
};

// ============================================================================
// STRUCT: CHARACTER_REPLACEMENT
//      Defines a single character replacement. The postprocessor can accept
//      arrays of these, and map all matched characters to their corresponding 
//      output strings. 
                      
struct CHARACTER_REPLACEMENT
{
    WCHAR cMatch;                           // Character to match
    LPCWSTR pwszReplace;                    // Text to replace with
};

// Limits.

const long g_nMaxInputBufferChunk = 4096;   // Input buffer chunking
const long g_nOutputBufferPadding = 16;     // Padding to add to output buffer
const long g_nMaxResidualLength = 32;       // See above


// ============================================================================
// CLASS: CProcessingStream
//      Output stream class. Implements the postprocessor code.
//
//      The postprocessor is heavily parameterized, through variables
//      defined in this class. An instance MUST set all of 
//      these parameters before calling the postprocessor. 
//
//      m_pEntitiesToReplace
//          Array of ENTITY_REPLACEMENT structures (see above)
//      m_nEntitiesToReplace
//          Number of elements in m_pEntitiesToReplace
//      m_pCharsToReplace
//          Array of CHARACTER_REPLACEMENT structures (see above)
//      m_nCharsToReplace
//          Number of elements in m_pCharsToReplace
//      m_pClosingTagsToRemove
//          Array of closing tags to remove
//      m_nClosingTagsToRemove
//          Number of elements in m_pClosingTagsToRemove
//      m_bExpandEmptyTags
//          Set to true to expand XML notation for empty tags ("/>")


class CProcessingStream : public IStream
{
public:
    CProcessingStream(IStream * pOutputStream,
                      asp::IResponse * pResponse,
                      const WCHAR * pwszStreamLanguage,
                      UINT uiCP);
#ifdef _DEBUG
    ~CProcessingStream();
#endif

    // IUnknown Methods
    STDMETHOD(QueryInterface)(REFIID riid, LPVOID FAR* ppvObj);
    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();

    // ISequentialStream Methods
    STDMETHOD(Read)(void __RPC_FAR *pv, ULONG cb, ULONG __RPC_FAR *pcbRead);
    STDMETHOD(Write)(const void __RPC_FAR *pv, ULONG cb, ULONG __RPC_FAR *pcbWritten);

    // IStream Methods
    STDMETHOD(Seek)(LARGE_INTEGER dlibMove, DWORD dwOrigin,
        ULARGE_INTEGER __RPC_FAR *plibNewPosition);
    STDMETHOD(SetSize)(ULARGE_INTEGER libNewSize);
    STDMETHOD(CopyTo)(IStream __RPC_FAR *pstm, ULARGE_INTEGER cb, 
        ULARGE_INTEGER __RPC_FAR *pcbRead, ULARGE_INTEGER __RPC_FAR *pcbWritten);
    STDMETHOD(Commit)(DWORD grfCommitFlags);
    STDMETHOD(Revert)(void);
    STDMETHOD(LockRegion)(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
    STDMETHOD(UnlockRegion)(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
    STDMETHOD(Stat)(STATSTG __RPC_FAR *pstatstg, DWORD grfStatFlag);
    STDMETHOD(Clone)(IStream __RPC_FAR *__RPC_FAR *ppstm);

public:
    struct SProcessingParameters
    {
        ENTITY_REPLACEMENT const * pEntitiesToReplace;
        int nEntitiesToReplace;
        CHARACTER_REPLACEMENT const * pCharsToReplace;
        int nCharsToReplace;
        CLOSING_TAG const * pClosingTagsToRemove;
        int nClosingTagsToRemove;
        bool bExpandEmptyTags;
    };

    struct SMIMEToStreamType
    {
        const WCHAR * pwszMIMEType;
        const SProcessingParameters * pProcessingParameters;
    };

private:
    UINT m_cRefs;
    CComPtr<IStream> m_pcomDestinationStream;
    CComPtr<asp::IResponse> m_pcomResponse;

public:
    HRESULT Initialize(IStream* pOutputStream);
    HRESULT ProcessorWrite(LPCWSTR pwszInData, long nDataLength);
    HRESULT ProcessorFlush();

private:
    ENTITY_REPLACEMENT const * m_pEntitiesToReplace;
    int m_nEntitiesToReplace;
    CHARACTER_REPLACEMENT const * m_pCharsToReplace;
    int m_nCharsToReplace;
    CLOSING_TAG const * m_pClosingTagsToRemove;
    int m_nClosingTagsToRemove;
    bool m_bExpandEmptyTags;
    bool m_fPostProcess;
    bool m_fFirstWrite;

    HRESULT ProcessorWriteChunk(LPCWSTR pwszChunk, long nChunkLength);
    HRESULT WriteToOutputBuffer(LPCWSTR pwszData, long nLength,
                long nInputDataRemaining, long* pnBufferUsed);
    HRESULT WriteResidualToOutputBuffer(long nInputDataRemaining,
                long* pnBufferUsed);
    HRESULT WriteToDestinationStream(const void __RPC_FAR *pv, ULONG cb, ULONG __RPC_FAR *pcbWritten);
    HRESULT WriteToDestinationObject(const void __RPC_FAR *pv, ULONG cb, ULONG __RPC_FAR *pcbWritten);
   #ifdef _DEBUG
    void VerifyProcessingParameters();
   #endif

    // Post-processor state. 
    enum PROCESSORSTATE
    {
        STATE_NORMAL = 0,
        STATE_STARTING_TAG,
        STATE_IN_ENTITY_REF,
        STATE_IN_CLOSING_TAG,
        STATE_IN_TAG_NAME,
        STATE_IN_TAG,
        STATE_CLOSING_EMPTY_TAG,
    };

    PROCESSORSTATE m_state;                 // Current state
    PROCESSORSTATE m_statePrevious;         // State before entering entity.
    HRESULT m_hrLast;                       // Last HRESULT returned
    int m_nQuoteState;                      // Current quote state
    WCHAR m_wszResidualBuffer[g_nMaxResidualLength + 1];
                                            // Buffer to keep tag/entity names
    long m_nResidualUsed;                   // Length of string in buffer
    WCHAR m_wszClosingTag[g_nMaxResidualLength + 3]; 
                                            // Buffer to keep current tag
    long m_nClosingTagLength;               // Length of string in buffer

    LPWSTR m_pwszOutputBuffer;              // Output buffer (_alloca)
    long m_nOutputBufferLength;             // Length of output buffer

    UINT m_uiCP;                            // Code page used for encoding

private:
    CProcessingStream();                    // disable default constructor
};


// HDML 1.0 Parameters
const CLOSING_TAG g_arrHDMLTagsToRemove[] = 
{
    L"ACTION",
    L"BR",
    L"CE",
    L"CENTER",
    L"IMG",
    L"LINE",
    L"RIGHT",
    L"TAB",
    L"WRAP",
};

const ENTITY_REPLACEMENT g_arrHDMLEntitiesToReplace[] = 
{
    { L"var", L"$" },
};

const CHARACTER_REPLACEMENT g_arrHDMLCharsToReplace[] =
{
    { L'$', L"&dol;" },
};

// WML 1.0 Parameters

const ENTITY_REPLACEMENT g_arrWMLEntitiesToReplace[] = 
{
    { L"var", L"$" },
};

const CHARACTER_REPLACEMENT g_arrWMLCharsToReplace[] =
{
    { L'$', L"$$" },
};

const CProcessingStream::SProcessingParameters g_HDMLProcessingParameters =
{
    /* pEntitiesToReplace */ g_arrHDMLEntitiesToReplace,
    /* nEntitiesToReplace */ COUNTOF(g_arrHDMLEntitiesToReplace),
    /* pCharsToReplace */ g_arrHDMLCharsToReplace,
    /* nCharsToReplace */ COUNTOF(g_arrHDMLCharsToReplace),
    /* pClosingTagsToRemove */ g_arrHDMLTagsToRemove,
    /* nClosingTagsToRemove */ COUNTOF(g_arrHDMLTagsToRemove),
    /* bExpandEmptyTags */ true
};

const CProcessingStream::SProcessingParameters g_WMLProcessingParameters =
{
    /* pEntitiesToReplace */ g_arrWMLEntitiesToReplace,
    /* nEntitiesToReplace */ COUNTOF(g_arrWMLEntitiesToReplace),
    /* pCharsToReplace */ g_arrWMLCharsToReplace,
    /* nCharsToReplace */ COUNTOF(g_arrWMLCharsToReplace),
    /* pClosingTagsToRemove */ NULL,
    /* nClosingTagsToRemove */ 0,
    /* bExpandEmptyTags */ false
};


// TODO: It would be nice to make this a static member of CProcessingStream,
// but getting the initialization to work right is tricky.
const CProcessingStream::SMIMEToStreamType g_aMIMEToStreamTypeMap[] =
    {
        { L"text/x-hdml", &g_HDMLProcessingParameters },
        { L"text/x-wap.wml", &g_WMLProcessingParameters },
        { L"text/vnd.wap.wml", &g_WMLProcessingParameters },
    };

const UINT g_cMIMEToStreamTypeMap = COUNTOF(g_aMIMEToStreamTypeMap);

// ============================================================================
// CProcessingStream::CProcessingStream
//      Constructor.

CProcessingStream::CProcessingStream
(
    IStream * pOutputStream,
    // [in] Pointer to stream to write output to
    asp::IResponse * pResponse,
    // [in] Pointer to Response object, used when pOutputStream is not provided
    const WCHAR * pwszStreamLanguage,
    // [in] MIME-type of stream
    UINT uiCP
    // [in] Code page for Encoding of stream
)
{
    UINT iWhichMapEntry;

    m_cRefs = 0;
    m_pcomDestinationStream = pOutputStream;
    m_pcomResponse = pResponse;
    m_fPostProcess = false;
    m_fFirstWrite = true;

    m_state = STATE_NORMAL;
    m_nResidualUsed = 0;
    m_hrLast = S_OK;

    m_pEntitiesToReplace = NULL;
    m_nEntitiesToReplace = 0;
    m_pCharsToReplace = NULL;
    m_nCharsToReplace = 0;
    m_pClosingTagsToRemove = NULL;
    m_nClosingTagsToRemove = 0;
    m_bExpandEmptyTags = false;

    m_uiCP = uiCP;


    for(iWhichMapEntry = 0; iWhichMapEntry < g_cMIMEToStreamTypeMap; ++iWhichMapEntry)
    {
        if(0 == lstrcmp(pwszStreamLanguage, g_aMIMEToStreamTypeMap[iWhichMapEntry].pwszMIMEType))
        {
            m_fPostProcess = true;
            m_pEntitiesToReplace =
                g_aMIMEToStreamTypeMap[iWhichMapEntry].pProcessingParameters->pEntitiesToReplace;
            m_nEntitiesToReplace =
                g_aMIMEToStreamTypeMap[iWhichMapEntry].pProcessingParameters->nEntitiesToReplace;
            m_pCharsToReplace =
                g_aMIMEToStreamTypeMap[iWhichMapEntry].pProcessingParameters->pCharsToReplace;
            m_nCharsToReplace =
                g_aMIMEToStreamTypeMap[iWhichMapEntry].pProcessingParameters->nCharsToReplace;
            m_pClosingTagsToRemove =
                g_aMIMEToStreamTypeMap[iWhichMapEntry].pProcessingParameters->pClosingTagsToRemove;
            m_nClosingTagsToRemove =
                g_aMIMEToStreamTypeMap[iWhichMapEntry].pProcessingParameters->nClosingTagsToRemove;
            m_bExpandEmptyTags =
                g_aMIMEToStreamTypeMap[iWhichMapEntry].pProcessingParameters->bExpandEmptyTags;
            break;
        }
    }

   #ifdef _DEBUG
    VerifyProcessingParameters ();
   #endif   
};

// ============================================================================
// CProcessingStream::~CProcessingStream
//      Destructor.

#ifdef _DEBUG
CProcessingStream::~CProcessingStream
(
)
{
    // If you are getting this assert, you should make sure you
    // flush the processor before this point.
    ASSERT (FAILED (m_hrLast) || m_nResidualUsed == 0);
}
#endif


// ============================================================================
// CProcessingStream::QueryInterface
//      Standard IUnknown method.

STDMETHODIMP
CProcessingStream::QueryInterface
(
    REFIID riid,
    LPVOID FAR* ppvObj
)
{
    HRESULT hr;

    ERRCHECK(NULL == ppvObj, E_POINTER);

    if(InlineIsEqualUnknown(riid))
    {
        *ppvObj = static_cast<IUnknown *>(this);
    }
    else if(InlineIsEqualGUID(riid, IID_ISequentialStream))
    {
        *ppvObj = static_cast<ISequentialStream *>(this);
    }
    else if(InlineIsEqualGUID(riid, IID_IStream))
    {
        *ppvObj = static_cast<IStream *>(this);
    }
    else
    {
        RETURNERR(E_NOINTERFACE);
    }

    this->AddRef();

    hr = S_OK;
Error:
    return hr;
}


// ============================================================================
// CProcessingStream::AddRef
//      Standard IUnknown method.

STDMETHODIMP_(ULONG)
CProcessingStream::AddRef
(
)
{
    return ++m_cRefs;
}


// ============================================================================
// CProcessingStream::AddRef
//      Standard IUnknown method.

STDMETHODIMP_(ULONG)
CProcessingStream::Release
(
)
{
    ULONG ulReturn;

    ASSERT(0 < m_cRefs);

    ulReturn = m_cRefs--;
    if(0 == ulReturn)
    {
        delete this;
    }
    return ulReturn;
}


// ============================================================================
// CProcessingStream::Read
//      Standard ISequentialStream method.

STDMETHODIMP
CProcessingStream::Read
( 
    void __RPC_FAR * /* pv */,
    ULONG /* cb */,
    ULONG __RPC_FAR * /* pcbRead */
)
{
    ERRORTRACE(E_NOTIMPL);
    return E_NOTIMPL;
}


// ============================================================================
// CProcessingStream::Write
//      Standard ISequentialStream method.

STDMETHODIMP
CProcessingStream::Write
( 
    const void __RPC_FAR *pv,
    ULONG cb,
    ULONG __RPC_FAR *pcbWritten
)
{
    HRESULT hr;
    const WCHAR chBOM = L'\xFEFF'; // Byte-order mark

    ERRCHECK(NULL == pv, STG_E_INVALIDPOINTER);

    if(m_fFirstWrite)
    {
        // We expect this to be the byte-order mark (BOM)
        ERRCHECK(cb != sizeof(WCHAR) ||
                 *reinterpret_cast<const WCHAR *>(pv) != chBOM,
                 STG_E_CANTSAVE);

        // Add necessary BOM according to code page setting
        if (m_uiCP == CP_UTF16 || m_uiCP == CP_UTF16FFFE) {
            const BYTE bom[2] = { 0xFF, 0xFE };
            const BYTE bomBig[2] = { 0xFE, 0xFF };  // Big-Endian
            ULONG cbWritten;

            hr = WriteToDestinationObject((m_uiCP == CP_UTF16)?bom:bomBig,
                                          2, &cbWritten);
            HRCHECK(FAILED(hr));
        }

        m_fFirstWrite = false;
    }
    else if(m_fPostProcess && cb > 0)
    {
        hr = this->ProcessorWrite(static_cast<LPCWSTR>(pv), (long)(cb / sizeof(WCHAR)));
        HRCHECK(FAILED(hr));
    }
    else if(cb > 0)
    {
        // Just pass right on through... no post processing needed.
        hr = WriteToDestinationStream(pv, cb, NULL);
        HRCHECK(FAILED(hr));
            
    }

    if (pcbWritten != NULL)
    {
        *pcbWritten = cb;
    }

    hr = S_OK;
  Error:
    return hr;
}


// ============================================================================
// CProcessingStream::Seek
//      Standard IStream method.

STDMETHODIMP
CProcessingStream::Seek
( 
    LARGE_INTEGER /* dlibMove */,
    DWORD /* dwOrigin */,
    ULARGE_INTEGER __RPC_FAR * /* plibNewPosition */
)
{
    ERRORTRACE(E_NOTIMPL);
    return E_NOTIMPL;
}


// ============================================================================
// CProcessingStream::SetSize
//      Standard IStream method.

STDMETHODIMP
CProcessingStream::SetSize
( 
    ULARGE_INTEGER /* libNewSize */
)
{
    ERRORTRACE(E_NOTIMPL);
    return E_NOTIMPL;
}


// ============================================================================
// CProcessingStream::CopyTo
//      Standard IStream method.

STDMETHODIMP
CProcessingStream::CopyTo
( 
    IStream __RPC_FAR * /* pstm */,
    ULARGE_INTEGER /* cb */,
    ULARGE_INTEGER __RPC_FAR * /* pcbRead */,
    ULARGE_INTEGER __RPC_FAR * /* pcbWritten */
)
{
    ERRORTRACE(E_NOTIMPL);
    return E_NOTIMPL;
}


// ============================================================================
// CProcessingStream::Commit
//      Standard IStream method.

STDMETHODIMP
CProcessingStream::Commit
( 
    DWORD /* grfCommitFlags */
)
{
    HRESULT hr;

    if (m_fPostProcess) {
        hr = this->ProcessorFlush();
        HRCHECK(FAILED(hr));
    }

    hr = S_OK;
  Error:
    return hr;
}


// ============================================================================
// CProcessingStream::Revert
//      Standard IStream method.

STDMETHODIMP
CProcessingStream::Revert
(
)
{
    ERRORTRACE(E_NOTIMPL);
    return E_NOTIMPL;
}


// ============================================================================
// CProcessingStream::LockRegion
//      Standard IStream method.

STDMETHODIMP
CProcessingStream::LockRegion
( 
    ULARGE_INTEGER /* libOffset */,
    ULARGE_INTEGER /* cb */,
    DWORD /* dwLockType */
)
{
    ERRORTRACE(E_NOTIMPL);
    return E_NOTIMPL;
}


// ============================================================================
// CProcessingStream::UnlockRegion
//      Standard IStream method.

STDMETHODIMP
CProcessingStream::UnlockRegion
( 
    ULARGE_INTEGER /* libOffset */,
    ULARGE_INTEGER /* cb */,
    DWORD /* dwLockType */
)
{
    ERRORTRACE(E_NOTIMPL);
    return E_NOTIMPL;
}


// ============================================================================
// CProcessingStream::Stat
//      Standard IStream method.

STDMETHODIMP
CProcessingStream::Stat
( 
    STATSTG __RPC_FAR * /* pstatstg */,
    DWORD /* grfStatFlag */
)
{
    ERRORTRACE(E_NOTIMPL);
    return E_NOTIMPL;
}


// ============================================================================
// CProcessingStream::Clone
//      Standard IStream method.

STDMETHODIMP
CProcessingStream::Clone
( 
    IStream __RPC_FAR *__RPC_FAR * /* ppstm */
)
{
    ERRORTRACE(E_NOTIMPL);
    return E_NOTIMPL;
}


// ============================================================================
// CreateProcessingStream
//      Create a new processing stream.

HRESULT
CreateProcessingStream
(
    IStream * pOutputStream,
    // [in] Stream to forward output to after processing
    asp::IResponse * pResponse,
    // [in] Response object, used if pOutputStream is not available
    const WCHAR * pwszStreamLanguage,
    // [in] MIME-type of stream
    UINT uiCP,
    // [in] Code page for encoding of stream
    IStream ** ppProcessingStream
    // [in] Storage for interface pointer to created processing stream
    // [out] Interface pointer to created processing stream
)
{
    HRESULT hr;
    CProcessingStream * pProcessingStream;

    ASSERT(NULL != pOutputStream || NULL != pResponse);
    ASSERT(NULL != ppProcessingStream);
    ASSERT(NULL != pwszStreamLanguage);

    pProcessingStream = new CProcessingStream(pOutputStream,
                                              pResponse,
                                              pwszStreamLanguage,
                                              uiCP);
    ERRCHECK(NULL == pProcessingStream, E_OUTOFMEMORY);
    pProcessingStream->AddRef();
    
    *ppProcessingStream = pProcessingStream;
    pProcessingStream = NULL;

    hr = S_OK;
  Error:
    SAFERELEASE(pProcessingStream);
    return hr;
}


// Callback functions to compare tag names and entity names.

int __fastcall CompareClosingTags(CLOSING_TAG const* pElem1,
                    CLOSING_TAG const* pElem2);
int __fastcall LookupClosingTags(LPCWSTR pwsz,
                    CLOSING_TAG const* pElem);
int __fastcall CompareEntityStructs(ENTITY_REPLACEMENT const* pElem1, 
                    ENTITY_REPLACEMENT const* pElem2);
int __fastcall LookupEntityStructs(LPCWSTR pwsz, 
                    ENTITY_REPLACEMENT const* pElem);


// ============================================================================
// CProcessingStream::ProcessorWrite
//      Processes a chunk of data, and writes the results to the output
//      stream. Call this once for each chunk of data to write.
//
//      NOTE: This function assumes it is being called by a trusted caller,
//      and does not have the same parameter semantics and error handling as 
//      IStream::Write. In particular, you cannot pass a length of 0,
//      and passing bad parameters does not return error messages. (see
//      further note below)
//
//      Because of the nature of processing, a call to this function
//      may not fully write all the data passed in. After all data has
//      been passed to this function, the caller should call ProcessorFlush
//      to write any remaining data. 
//
//      Returns HRESULT indicating success.

HRESULT
CProcessingStream::ProcessorWrite(
    LPCWSTR pwszInData,                     // [in] Pointer to data
    long nDataLength)                       // [in] Length of data, in characters
{
    // We'll just do ASSERTs here - if this is being called from
    // IStream::Write, that function should handle the cases:
    //      pszInData == NULL (error, return STG_E_INVALIDPOINTER)
    //      nDataLength == 0 (OK, return S_OK)

    ASSERT (pwszInData != NULL);
    ASSERT (nDataLength > 0);
    ASSERT (!IsBadReadPtr (pwszInData, nDataLength));

    // If there was a previous failure, we can't recover.

    if (FAILED(m_hrLast))
    {
        m_hrLast = E_FAIL;
        return m_hrLast;
    }

    // BUFFER MANAGEMENT NOTE
    //
    // The postprocessor can be called with buffers of arbitrary length.
    // From this, it needs to output data in reasonably sized buffers.
    // In order to accomplish this, without allocating any additional
    // memory, the postprocessor uses a variable-length buffer, allocated
    // on the stack. 
    //
    // There is a limit on the size of the output buffer. To handle input
    // buffers that are bigger than the output buffer, this function
    // breaks the input buffer into suitable "chunks", and calls 
    // ProcessorWriteChunk to handle each chunk. 
    //
    // To write to the buffer without a lot of checking, some simple
    // heuristics are followed. At any time, the output buffer contains
    // at least enough room to allow all remaining characters in the 
    // input chunk to be copied unmodified. This allows simple character
    // copies to be done without checking for buffer overflow. Whenever
    // a more complex write or substitution is required, the more complex
    // WriteToOutputBuffer function is used, which preserves the 
    // above condition, flushing the buffer if necessary.
    //
    // Whenever the input goes into a tag or entity, the input is copied
    // instead to a residual buffer. After the entire tag name or entity
    // has been read in, we can write or discard the contents of the
    // residual buffer. The state of the residual buffer is preserved
    // across calls to ProcessorWrite, allowing tags and entities to
    // span multiple buffers. Whenever the residual buffer is filled up,
    // the code flushes the buffer, and effectively skips special
    // processing for the tag or entity.

    long nInputBufferChunk;
    if (nDataLength < g_nMaxInputBufferChunk)
        nInputBufferChunk = nDataLength;
    else
        nInputBufferChunk = g_nMaxInputBufferChunk;

    m_nOutputBufferLength = nInputBufferChunk + 
                                nInputBufferChunk / 2 +
                                g_nMaxResidualLength +
                                g_nOutputBufferPadding;

    // Allocate output buffer on stack.

    m_pwszOutputBuffer = (LPWSTR)_alloca (m_nOutputBufferLength * sizeof(WCHAR));
    ASSERT (m_pwszOutputBuffer != NULL);

    // Go through each chunk.

    HRESULT hr = S_OK;
    for (long nChunk = (nDataLength + nInputBufferChunk - 1) / nInputBufferChunk; 
         nChunk > 0 && hr == S_OK; 
         nChunk--)
    {
        hr = ProcessorWriteChunk (pwszInData,
                    (nChunk == 1) ? nDataLength : nInputBufferChunk);
        pwszInData += nInputBufferChunk;
        nDataLength -= nInputBufferChunk;
    }

    m_hrLast = hr;
    return hr;
}

// ============================================================================
// CProcessingStream::ProcessorFlush
//      Writes any pending data to the output stream. Call this once after
//      passing all data to the document, but before calling Uninitialize.
//
//      NOTE: Calling this function causes the postprocessor to lose state,
//      so any subsequent calls to ProcessorWrite will have unpredictable
//      results.
//
//      Returns HRESULT indicating success.

HRESULT
CProcessingStream::ProcessorFlush()
{
    if (FAILED (m_hrLast))
    {
        // If there was a previous failure, we can't recover.
        m_hrLast = E_FAIL;
    }
    else if (m_nResidualUsed > 0)
    {
        m_hrLast = WriteToDestinationStream (m_wszResidualBuffer, 
                        m_nResidualUsed * sizeof(WCHAR), NULL);
        m_nResidualUsed = 0;
    }
    return m_hrLast;
}

// ============================================================================
// CProcessingStream::ProcessorWriteChunk
//      Internal function that processes and writes data in chunks. For
//      an explanation of chunks, see ProcessorWrite above.  
//
//      Returns HRESULT indicating success.

HRESULT 
CProcessingStream::ProcessorWriteChunk(
    LPCWSTR pwszChunk,                      // [in] Pointer to chunk
    long nChunkLength)                      // [in] Length of chunk, in characters
{
    ASSERT (m_hrLast == S_OK);
    ASSERT (pwszChunk != NULL);
    ASSERT (nChunkLength < m_nOutputBufferLength);

    // Initialize the output buffer, so that it is empty.
    LPWSTR pwszBuffer = m_pwszOutputBuffer;
    long nOutputBufferUsed = 0;

    HRESULT hr = S_OK;

    PROCESSORSTATE state = m_state;
    while (hr == S_OK && nChunkLength > 0)
    {
        WCHAR c = *pwszChunk;
        bool bRepeatCharacter = false;

        if (state == STATE_NORMAL || state == STATE_IN_TAG)
        {
            // Check for character replacements.
            // TODO: Going through a loop on each character may
            // be too slow - could optimize by creating a lookup 
            // table for characters less than 256.

            int i = m_nCharsToReplace; 
            while (--i >= 0)
            {
                if (m_pCharsToReplace[i].cMatch == c)
                {
                    break;
                }
            }

            if (i >= 0)
            {
                // Found a match, substitute the replacement string,
                // skip this character, and continue.

                ASSERT (m_nResidualUsed == 0);
                hr = WriteToOutputBuffer (m_pCharsToReplace[i].pwszReplace,
                    -1, nChunkLength - 1, &nOutputBufferUsed);
                pwszChunk++;
                nChunkLength--;
                continue;
            }
        }

        switch (state)
        {
            case STATE_NORMAL:
                // This is the state the postprocessor is most
                // often in. It indicates that the input is outside
                // any tag or entity. There should also be no residual
                // data when we are in this state.
                ASSERT (m_nResidualUsed == 0);
                if (c == L'<' || c == L'&')
                {
                    // Enter a tag or entity.
                    m_wszResidualBuffer[0] = c;
                    m_nResidualUsed = 1;
                    m_nQuoteState = 0;
                    state = c == L'<' ? STATE_STARTING_TAG : STATE_IN_ENTITY_REF;
                    m_statePrevious = STATE_NORMAL;
                }
                else
                {
                    pwszBuffer[nOutputBufferUsed++] = c;
                }
                break;

            case STATE_STARTING_TAG:
                // This state indicates that we have just read a <
                // character, indicating the start of a tag. Subsequent
                // characters will determine the type of tag.
                // BUGBUG - we may have to account for CDATA and comment
                // sections here. Such sections may have a mismatched
                // number of quotes, and the postprocessor will not
                // pick up the tag closing.
                ASSERT (m_nResidualUsed == 1);
                m_wszResidualBuffer[m_nResidualUsed++] = c;
                if (c == L'/')
                {
                    state = STATE_IN_CLOSING_TAG;
                }
                else 
                {
                    // We are entering a tag name. We only need special
                    // processing for this if we are expanding empty tags.
                    if (m_bExpandEmptyTags)
                    {
                        state = STATE_IN_TAG_NAME;
                    }
                    else
                    {
                        pwszBuffer[nOutputBufferUsed++] = m_wszResidualBuffer[0];
                        pwszBuffer[nOutputBufferUsed++] = c;
                        m_nResidualUsed = 0;
                        state = STATE_IN_TAG;
                    }
                }
                break;

            case STATE_IN_CLOSING_TAG:
                // This state indicates that we are reading a closing
                // tag. We need to save the entire tag name in the
                // residual buffer, so that we can check if the tag
                // should be removed or not.
                if (c == L'>' || m_nResidualUsed == g_nMaxResidualLength)
                {
                    int nLookup;
                    if (m_nResidualUsed < g_nMaxResidualLength)
                    {
                        m_wszResidualBuffer[m_nResidualUsed] = L'\0';
                        nLookup = LookupSortedArray (m_wszResidualBuffer + 2,
                                        m_pClosingTagsToRemove,
                                        m_nClosingTagsToRemove,
                                        LookupClosingTags);
                    }
                    else
                    {
                        nLookup = -1;
                    }

                    // Only write out the tag if it's not on the list
                    // of closing tags to remove.
                    if (nLookup == -1)
                    {
                        hr = WriteResidualToOutputBuffer (nChunkLength, 
                                    &nOutputBufferUsed);
                        bRepeatCharacter = true;
                    }
                    m_nResidualUsed = 0;
                    state = STATE_NORMAL;
                }
                else
                {
                    m_wszResidualBuffer[m_nResidualUsed++] = c;
                }
                break;

            case STATE_IN_TAG_NAME:
                // This state indicates that we are reading the name of
                // a tag. We need to save the tag name in the residual
                // buffer, so that we can create a closing tag, if necessary.
                if ((c <= L' ' && CHAR_IS_WHITESPACE(c)) || c == L'>' || c == L'/')
                {
                    // Save the tag name somewhere safe.
                    m_wszClosingTag[0] = L'<';
                    m_wszClosingTag[1] = L'/';
                    CopyMemory (&m_wszClosingTag[2], 
                        m_wszResidualBuffer + 1, (m_nResidualUsed - 1) * sizeof(WCHAR));
                    m_nClosingTagLength = m_nResidualUsed - 1;
                    hr = WriteResidualToOutputBuffer (nChunkLength, &nOutputBufferUsed);
                    m_nResidualUsed = 0;
                    bRepeatCharacter = true;
                    state = STATE_IN_TAG;
                }
                else if (m_nResidualUsed == g_nMaxResidualLength)
                {
                    // Tag name is too long - skip special processing.
                    hr = WriteResidualToOutputBuffer (nChunkLength, &nOutputBufferUsed);
                    m_nResidualUsed = 0;
                    m_nClosingTagLength = 0;
                    bRepeatCharacter = true;
                    state = STATE_IN_TAG;
                }
                else
                {
                    m_wszResidualBuffer[m_nResidualUsed++] = c;
                }
                break;

            case STATE_IN_TAG:
                // This state indicates that we are reading a tag.
                // We need to do special processing to check quote state,
                // and to look for the end of tag, either as a "/>" or a ">".

                pwszBuffer[nOutputBufferUsed++] = c;

                if (c == L'&')
                {
                    // Enter an entity.
                    nOutputBufferUsed--;
                    m_wszResidualBuffer[0] = c;
                    m_nResidualUsed = 1;
                    state = STATE_IN_ENTITY_REF;
                    m_statePrevious = STATE_IN_TAG;
                }
                else if (c == L'\'')
                {
                    m_nQuoteState = 
                        QUOTE_SINGLE_QUOTE_TRANSITION (m_nQuoteState);
                }
                else if (c == L'\"')
                {
                    m_nQuoteState = 
                        QUOTE_SINGLE_QUOTE_TRANSITION (m_nQuoteState);
                }
                else if (c == L'/' && m_nQuoteState == 0 && m_bExpandEmptyTags)
                {
                    // This is an empty tag. 
                    nOutputBufferUsed--;
                    state = STATE_CLOSING_EMPTY_TAG;
                }
                else if (c == L'>' && m_nQuoteState == 0)
                {
                    state = STATE_NORMAL;
                }
                break;

            case STATE_IN_ENTITY_REF:
                // This state indicates that we are processing an entity
                // reference. We need to store the entity name in the
                // residual buffer, so that we can check if we need to
                // replace it with some other string. When we reach a 
                // semicolon, the entity is processed.
                if (c == L';' || m_nResidualUsed == g_nMaxResidualLength)
                {
                    int nLookup;
                    if (m_nResidualUsed < g_nMaxResidualLength)
                    {
                        m_wszResidualBuffer[m_nResidualUsed] = L'\0';
                        nLookup = LookupSortedArray (m_wszResidualBuffer + 1,
                                        m_pEntitiesToReplace,
                                        m_nEntitiesToReplace,
                                        LookupEntityStructs);
                    }
                    else
                    {
                        nLookup = -1;
                    }

                    if (nLookup != -1)
                    {
                        // Replace with another string.
                        hr = WriteToOutputBuffer (
                                m_pEntitiesToReplace[nLookup].pwszReplace,
                                -1, nChunkLength - 1, &nOutputBufferUsed);
                    }
                    else
                    {
                        // Just write out the entity.
                        hr = WriteResidualToOutputBuffer (nChunkLength, 
                                    &nOutputBufferUsed);
                        bRepeatCharacter = true;
                    }
                    m_nResidualUsed = 0;
                    state = m_statePrevious;
                }
                else
                {
                    m_wszResidualBuffer[m_nResidualUsed++] = c;
                }
                break;

            case STATE_CLOSING_EMPTY_TAG:
                // Closing an XML empty tag, denoted with the "/>"
                // syntax. If requested, we can replace this with a
                // real closing tag.
                if (c == L'>')
                {
                    if (m_nClosingTagLength > 0)
                    {
                        pwszBuffer[nOutputBufferUsed++] = c;
                        m_wszClosingTag[m_nClosingTagLength + 2] = L'\0';
                        if (LookupSortedArray (m_wszClosingTag + 2,
                                m_pClosingTagsToRemove,
                                m_nClosingTagsToRemove,
                                LookupClosingTags) == -1)
                        {
                            m_wszClosingTag[m_nClosingTagLength + 2] = L'>';
                            hr = WriteToOutputBuffer (m_wszClosingTag, 
                                m_nClosingTagLength + 3, 
                                nChunkLength - 1, 
                                &nOutputBufferUsed);
                        }
                    }
                    else
                    {
                        // The tag name was too long, so we need to 
                        // keep the /> syntax.

                        ASSERT (false);
                        pwszBuffer[nOutputBufferUsed++] = L'/';
                        pwszBuffer[nOutputBufferUsed++] = c;
                    }
                    state = STATE_NORMAL;
                }
                else
                {
                    // Something wrong with empty tag syntax.
                    // BUGBUG - this will also fail if comments or
                    // CDATA sections have / characters in them!
                    hr = E_FAIL;
                }
                break;
        }

        if (!bRepeatCharacter)
        {
            pwszChunk++;
            nChunkLength--;
        }

        // Validate output buffer condition.
        ASSERT (m_nOutputBufferLength - nOutputBufferUsed >= nChunkLength);
    }

    // Flush the output buffer.
    if (hr == S_OK)
    {
        if (nOutputBufferUsed > 0)
        {
            hr = WriteToDestinationStream (m_pwszOutputBuffer, 
                        nOutputBufferUsed * sizeof(WCHAR), NULL);
        }

        m_state = state;
    }

    return hr;
}

// ============================================================================
// CProcessingStream::WriteToOutputBuffer
//      Writes a variable length string to the output buffer. For an
//      explanation of output buffering, see ProcessorWrite above. 
//
//      Returns HRESULT indicating success.

HRESULT 
CProcessingStream::WriteToOutputBuffer(
    LPCWSTR pwszData,                       // [in] Data to write
    long nLength,                           // [in] Length of data
    long nInputDataRemaining,               // [in] Input data remaining
    long* pnBufferUsed)                     // [in][out] Amount written
{
    HRESULT hr = S_OK;
    long nBufferUsed = *pnBufferUsed;

    if (nLength < 0)
    {
        nLength = lstrlen (pwszData);
    }

    if (nLength > 0)
    {
        long nBufferRemaining = m_nOutputBufferLength - nBufferUsed;

        // Validate output buffer condition.
        ASSERT (nBufferRemaining >= nInputDataRemaining);

        if (nLength >= m_nOutputBufferLength)
        {
            // Very long strings, that wond require mntiple flushes of
            // the buffer, are instead written out without buffering.
            
            // First we need to flush the buffer.
            if (nBufferUsed > 0)
            {
                hr = WriteToDestinationStream (m_pwszOutputBuffer,
                            nBufferUsed * sizeof(WCHAR), NULL);
                HRCHECK(FAILED(hr));
            }

            // Now write the string.
            hr = WriteToDestinationStream (pwszData,
                        nLength * sizeof(WCHAR), NULL);
            HRCHECK(FAILED(hr));

            nBufferUsed = 0;
        }
        else if (nBufferRemaining >= nInputDataRemaining + nLength)
        {
            // String will fit without breaking buffer condition.
            CopyMemory (m_pwszOutputBuffer + nBufferUsed, pwszData, 
                nLength * sizeof(WCHAR));

            nBufferUsed += nLength;
        }
        else
        {
            // Fill up the buffer.
            CopyMemory (m_pwszOutputBuffer + nBufferUsed, pwszData,
                nBufferRemaining * sizeof(WCHAR));

            // Flush the buffer.
            hr = WriteToDestinationStream (m_pwszOutputBuffer,
                    m_nOutputBufferLength * sizeof(WCHAR), NULL);
            HRCHECK(FAILED(hr));

            nLength -= nBufferRemaining;

            // Copy the rest.
            CopyMemory (m_pwszOutputBuffer, pwszData + nBufferRemaining,
                    nLength * sizeof(WCHAR));

            nBufferUsed = nLength;
        }

        *pnBufferUsed = nBufferUsed;

        // Validate output buffer condition again.
        ASSERT (m_nOutputBufferLength - nBufferUsed >= nInputDataRemaining);
    }
    else
    {
        hr = S_OK;
    }

  Error:
    return hr;
}

// ============================================================================
// CProcessingStream::WriteResidualToOutputBuffer
//      Writes any data in the residual buffer to the output buffer. For an
//      explanation of output buffering, see ProcessorWrite above. 
//
//      Returns HRESULT indicating success.

HRESULT 
CProcessingStream::WriteResidualToOutputBuffer(
   long nInputDataRemaining,
   long* pnBufferUsed)
{
    return WriteToOutputBuffer (m_wszResidualBuffer, m_nResidualUsed,
                nInputDataRemaining, pnBufferUsed);
}

// CProcessingStream::WriteToDestinationStream
//      Writes data to the ultimate destination stream. This method provides a
//      chokepoint for debugging.
//
//      Returns HRESULT indicating success.

HRESULT
CProcessingStream::WriteToDestinationStream
(
    const void __RPC_FAR *pv,
    ULONG cb,
    ULONG __RPC_FAR *pcbWritten
)
{
    HRESULT hr;
    UINT cbNeeded;
    CHAR * pszBuffer = NULL;
    INT iResult;

    ASSERT(0 == cb % 2);

    if (m_uiCP == CP_UTF16)  // Unicode is written out directly
    {
        hr = WriteToDestinationObject(pv, cb, pcbWritten);
        HRCHECK(FAILED(hr));

        RETURNERR(S_OK);
    }
    else if (m_uiCP == CP_UTF16FFFE)  // Unicode in Big-Endian
    {
        pszBuffer = (LPSTR) _alloca (cb);
        ERRCHECK(NULL == pszBuffer, E_OUTOFMEMORY);

        vLittleEndianToBigEndian(reinterpret_cast<LPCWSTR> (pv),
                                 cb / 2,
                                 reinterpret_cast<BYTE*> (pszBuffer));
        hr = WriteToDestinationObject(pszBuffer, cb, pcbWritten);
        HRCHECK(FAILED(hr));

        RETURNERR(S_OK);
    }

    // REVIEW: This may be a source of a performance problem
    cbNeeded = WideCharToMultiByte(
        m_uiCP,
        0,
        reinterpret_cast<const WCHAR *>(pv),
        cb / sizeof(WCHAR),
        NULL,
        0,
        NULL,
        NULL);
    ERRCHECK(0 == cbNeeded, HRESULT_FROM_WIN32(GetLastError()));

    pszBuffer = (LPSTR) _alloca (cbNeeded);
    ERRCHECK(NULL == pszBuffer, E_OUTOFMEMORY);

    iResult = WideCharToMultiByte(
        m_uiCP,
        0,
        reinterpret_cast<const WCHAR *>(pv),
        cb / sizeof(WCHAR),
        pszBuffer,
        cbNeeded,
        NULL,
        NULL);
    ERRCHECK(0 == iResult, HRESULT_FROM_WIN32(GetLastError()));

    hr = WriteToDestinationObject(pszBuffer, cbNeeded, pcbWritten);
    HRCHECK(FAILED(hr));
    
    hr = S_OK;
  Error:
    return hr;
}

// CProcessingStream::WriteToDestinationObject
//      Writes data to the ultimate destination object, either the stream or the
//      Response.  The stream takes a higher priority if available.
//
//      Returns HRESULT indicating success.

HRESULT
CProcessingStream::WriteToDestinationObject
(
    const void __RPC_FAR *pv,
    ULONG cb,
    ULONG __RPC_FAR *pcbWritten
)
{
    HRESULT hr;

    if (m_pcomDestinationStream) {
        hr = m_pcomDestinationStream->Write(pv, cb, pcbWritten);
        HRCHECK(FAILED(hr));
    } else {
        VARIANT vValue;
        SAFEARRAY sarray = {
            1, FADF_FIXEDSIZE, 1, 0,
            const_cast<void*> (pv),
            {cb, 0}
        };

        ASSERT(false == !m_pcomResponse);

        V_VT(&vValue) = VT_ARRAY | VT_UI1;
        V_ARRAY(&vValue) = &sarray;

        hr = m_pcomResponse->BinaryWrite(vValue);
        HRCHECK(FAILED(hr));
    }

    hr = S_OK;
  Error:
    return hr;
}


// ============================================================================
// CompareClosingTags
//      Compares two closing tags (really just strings).
//
//      Returns -1 if pElem1 < pElem2, 0 if pElem1 == pElem2, 
//      and 1 if pElem1 > pElem2.

int __fastcall 
CompareClosingTags(
    CLOSING_TAG const* pElem1, 
    CLOSING_TAG const* pElem2)
{
    return lstrcmp (*pElem1, *pElem2);
}

// ============================================================================
// LookupClosingTags
//      Compares a string to a closing tag.  Case insensitive since
//      the markups that require closing tags "/"'s to be removed are
//      case insensitive (i.e. HDML).
//
//      Returns -1 if pwsz < pElem, 0 if pwsz == pElem, and 1 if
//      pwsz > pElem. 

int __fastcall 
LookupClosingTags(
    LPCWSTR pwsz,
    CLOSING_TAG const* pElem)
{
    return lstrcmpi (pwsz, *pElem);
}

// ============================================================================
// CompareEntityStructs
//      Compares two ENTITY_REPLACEMENT structures by the entity name.
//
//      Returns -1 if pElem1 < pElem2, 0 if pElem1 == pElem2, 
//      and 1 if pElem1 > pElem2.

int __fastcall 
CompareEntityStructs(
    ENTITY_REPLACEMENT const* pElem1, 
    ENTITY_REPLACEMENT const* pElem2)
{
    return lstrcmp (pElem1->pwszName, pElem2->pwszName);
}

// ============================================================================
// LookupEntityStructs
//      Compares a string to the entity name of an ENTITY_REPLACEMENT structure.
//
//      Returns -1 if pwsz < pElem, 0 if pwsz == pElem, and 1 if pwsz > pElem.

int __fastcall 
LookupEntityStructs(
    LPCWSTR pwsz, 
    ENTITY_REPLACEMENT const* pElem)
{
    return lstrcmp (pwsz, pElem->pwszName);
}


// ============================================================================
// VerifyProcessingParameters
//      Debug-only function to validate whether the processing parameters are
//      legitimate. If any of the asserts in this function fail, there is an
//      error in the code.

#ifdef _DEBUG

void 
CProcessingStream::VerifyProcessingParameters()
{
    // Verify that all lookup arrays are properly sorted.

    // If this assert fails, the array of closing tags to be removed is not
    // properly sorted!
    ASSERT (VerifySortedArray (m_pClosingTagsToRemove,
                m_nClosingTagsToRemove,
                CompareClosingTags));

    // If this assert fails, the array of entities to be replaced is not
    // properly sorted!
    ASSERT (VerifySortedArray (m_pEntitiesToReplace,
                m_nEntitiesToReplace,
                CompareEntityStructs));

    // Check input strings.

    int i;
    
    for (i = 0; i < m_nClosingTagsToRemove; i++)
    {
        // If this assert fails, the given closing tag name is too long!
        ASSERT (lstrlen (m_pClosingTagsToRemove[i]) <= 
            g_nMaxResidualLength - 3);
    }

    for (i = 0; i < m_nEntitiesToReplace; i++)
    {
        // If this assert fails, the given entity name is too long!
        ASSERT (lstrlen (m_pEntitiesToReplace[i].pwszName) <= 
            g_nMaxResidualLength - 2);
    }

    for (i = 0; i < m_nCharsToReplace; i++)
    {
        WCHAR c = m_pCharsToReplace[i].cMatch;

        // If this assert fails, the character is not allowed.
        ASSERT (c != L'<' && c != L'&');
    }
}

#endif
