// ============================================================================
// FILE: charset.h
//
//      Helper functions for code page manipulation
//
// Copyright (c) 1999-2000 Microsoft Corporation. All rights reserved.

#pragma once

const UINT CP_UNDEFINED = UINT(-1);
const UINT CP_UTF16 = 1200;
const UINT CP_UTF16FFFE = 1201;  // Big Endian

extern UINT uiCodePageFromCharset(LPCWSTR pwszCharset);

extern void vLittleEndianToBigEndian(LPCWSTR pwszLittleEndian,
                                     UINT cch,
                                     BYTE *pbBigEndian);
