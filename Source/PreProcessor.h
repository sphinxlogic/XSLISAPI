// ============================================================================
// FILE: PreProcessor.h
//
//        Declarations needed for actual scanner/preprocessor.
//        (This is used by the ASPPreprocessor COM object.)
//
// Copyright (c) 1999-2000 Microsoft Corporation. All rights reserved.

#pragma once

// ============================================================================
// CLASS: Writer
//         Interface for generic writing of results.

class Writer {
  public:
    virtual HRESULT Out(LPCSTR psz, int nLength) = 0;

    // Wrapper around Out, that verifies nul-terminated constants.
    HRESULT ConstOut(LPCSTR psz, int nLength)
        { ASSERT (lstrlenA (psz) == nLength); return Out (psz, nLength); }

};

HRESULT ScanAndPreprocess(LPCSTR pszBuffer,
                          DWORD dwBufferLength,
                          Writer & writer);

