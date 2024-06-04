// ============================================================================
// FILE: charset.cpp
//
//      Helper functions for code page look up
//
// Copyright (c) 1999-2000 Microsoft Corporation. All rights reserved.

#include "StdAfx.h"
#include "charset.h"

//
// We handle Windows code pages using NLS functions.  This table provides a
// mapping from charset string to windows code page, where the charset strings
// are those recognized by IE 4 and 5.  The charset strings are described in
// the MSDN article titled "Character Set Recognition"
//
// !!PLEASE NOTE!! The entries are listed in increasing order sorted by charset
//                 string (case-insensitive).  So if more entries are added,
//                 they must be in the right order.
//
struct EncodingEntry
{
    UINT codepage;
    LPCWSTR charset;
};

const EncodingEntry charsetInfo[] = 
{
    { 1256,  L"ASMO-708" },

    { 950,   L"big5" },

    { 1254,  L"CP1026" },
    { 1251,  L"cp866" },
    { 1250,  L"CP870" },
    { 932,   L"csISO2022JP" },

    { 1256,  L"DOS-720" },
    { 1255,  L"DOS-862" },

    { 1252,  L"ebcdic-cp-us" },
    { 936,   L"EUC-CN" },
    { 932,   L"euc-jp" },
    { 949,   L"euc-kr" },

    { 936,   L"gb2312" },

    { 936,   L"hz-gb-2312" },

    { 1252,  L"IBM437" },
    { 1253,  L"ibm737" },
    { 1257,  L"ibm775" },
    { 1252,  L"ibm850" },
    { 1250,  L"ibm852" },
    { 1254,  L"ibm857" },
    { 1252,  L"ibm861" },
    { 1253,  L"ibm869" },
    { 932,   L"iso-2022-jp" },
    { 949,   L"iso-2022-kr" },
    { 1252,  L"iso-8859-1" },
    { 1252,  L"iso-8859-15" },
    { 1250,  L"iso-8859-2" },
    { 1254,  L"iso-8859-3" },
    { 1257,  L"iso-8859-4" },
    { 1251,  L"iso-8859-5" },
    { 1256,  L"iso-8859-6" },
    { 1253,  L"iso-8859-7" },
    { 1255,  L"iso-8859-8" },
    { 1255,  L"iso-8859-8-i" },
    { 1254,  L"iso-8859-9" },

    { 1361,  L"Johab" },

    { 1251,  L"koi8-r" },
    { 1251,  L"koi8-u" },
    { 949,   L"ks_c_5601-1987" },

    { 1252,  L"macintosh" },

    { 932,   L"shift_jis" },

    { CP_UTF16,     L"ucs-2" },        // Not in original table
    { CP_UTF16,     L"unicode" },
    { CP_UTF16FFFE, L"unicodeFFFE" },  // Big-Endian
    { 1252,         L"us-ascii" },
    { CP_UTF16,     L"utf-16" },       // Not in original table
    { CP_UTF7,      L"utf-7" },        // adjusted according to winnls.h
    { CP_UTF8,      L"utf-8" },        // adjusted according to winnls.h

    { 1250,  L"windows-1250" },
    { 1251,  L"windows-1251" },
    { 1252,  L"Windows-1252" },
    { 1253,  L"windows-1253" },
    { 1254,  L"windows-1254" },
    { 1255,  L"windows-1255" },
    { 1256,  L"windows-1256" },
    { 1257,  L"windows-1257" },
    { 1258,  L"windows-1258" },
    { 874,   L"windows-874" },

    { 950,   L"x-Chinese-CNS" },
    { 950,   L"x-Chinese-Eten" },
    { 1256,  L"x-EBCDIC-Arabic" },
    { 1252,  L"x-ebcdic-cp-us-euro" },
    { 1251,  L"x-EBCDIC-CyrillicRussian" },
    { 1251,  L"x-EBCDIC-CyrillicSerbianBulgarian" },
    { 1252,  L"x-EBCDIC-DenmarkNorway" },
    { 1252,  L"x-ebcdic-denmarknorway-euro" },
    { 1252,  L"x-EBCDIC-FinlandSweden" },
    { 1252,  L"x-ebcdic-finlandsweden-euro" },
    { 1252,  L"x-ebcdic-france-euro" },
    { 1252,  L"x-EBCDIC-Germany" },
    { 1252,  L"x-ebcdic-germany-euro" },
    { 1253,  L"x-EBCDIC-Greek" },
    { 1253,  L"x-EBCDIC-GreekModern" },
    { 1255,  L"x-EBCDIC-Hebrew" },
    { 1252,  L"x-EBCDIC-Icelandic" },
    { 1252,  L"x-ebcdic-icelandic-euro" },
    { 1252,  L"x-ebcdic-international-euro" },
    { 1252,  L"x-EBCDIC-Italy" },
    { 1252,  L"x-ebcdic-italy-euro" },
    { 932,   L"x-EBCDIC-JapaneseAndJapaneseLatin" },
    { 932,   L"x-EBCDIC-JapaneseAndKana" },
    { 932,   L"x-EBCDIC-JapaneseAndUSCanada" },
    { 932,   L"x-EBCDIC-JapaneseKatakana" },
    { 949,   L"x-EBCDIC-KoreanAndKoreanExtended" },
    { 949,   L"x-EBCDIC-KoreanExtended" },
    { 936,   L"x-EBCDIC-SimplifiedChinese" },
    { 1252,  L"X-EBCDIC-Spain" },
    { 1252,  L"x-ebcdic-spain-euro" },
    { 874,   L"x-EBCDIC-Thai" },
    { 950,   L"x-EBCDIC-TraditionalChinese" },
    { 1254,  L"x-EBCDIC-Turkish" },
    { 1252,  L"x-EBCDIC-UK" },
    { 1252,  L"x-ebcdic-uk-euro" },

    { 1252,  L"x-Europa" },
    { 1252,  L"x-IA5" },
    { 1252,  L"x-IA5-German" },
    { 1252,  L"x-IA5-Norwegian" },
    { 1252,  L"x-IA5-Swedish" },

    { 57006, L"x-iscii-as" },
    { 57003, L"x-iscii-be" },
    { 57002, L"x-iscii-de" },
    { 57010, L"x-iscii-gu" },
    { 57008, L"x-iscii-ka" },
    { 57009, L"x-iscii-ma" },
    { 57007, L"x-iscii-or" },
    { 57011, L"x-iscii-pa" },
    { 57004, L"x-iscii-ta" },
    { 57005, L"x-iscii-te" },
    
    { 1256,  L"x-mac-arabic" },
    { 1250,  L"x-mac-ce" },
    { 936,   L"x-mac-chinesesimp" },
    { 950,   L"x-mac-chinesetrad" },
    { 1251,  L"x-mac-cyrillic" },
    { 1253,  L"x-mac-greek" },
    { 1255,  L"x-mac-hebrew" },
    { 1252,  L"x-mac-icelandic" },
    { 932,   L"x-mac-japanese" },
    { 949,   L"x-mac-korean" },    
    { 1254,  L"x-mac-turkish" },
};

const UINT cCharInfo = COUNTOF(charsetInfo);


// ============================================================================
// uiCodePageFromCharset
//      Given a charset string, determine the code page value from the table

UINT
uiCodePageFromCharset(LPCWSTR pwszCharset)
{
    int iBot = 0;
    int iTop = cCharInfo - 1;

    ASSERT(pwszCharset);

/* Debug statements for checking the order of the mapping table entries
    for (int i = 0; i < iTop; i++) {
        if (lstrcmpi(charsetInfo[i].charset, charsetInfo[i+1].charset) > 0)
            ASSERT(false);  // Wrong order!
    }
*/

    while (true)
    {
        int iMid = (iTop + iBot) / 2;
        int iCmp = lstrcmpi(pwszCharset, charsetInfo[iMid].charset);

        if (iCmp < 0) {
            iTop = iMid - 1;
        }
        else if (iCmp > 0) {
            iBot = iMid + 1;
        }
        else {
            return charsetInfo[iMid].codepage;
        }

        if (iBot > iTop) {
            return CP_UNDEFINED;
        }
    }
}


// ============================================================================
// vLittleEndianToBigEndian
//      Convert Unicode string from Little-Endian format to Big-Endian format.
//      Assume the caller provides a sufficient return buffer

void
vLittleEndianToBigEndian(LPCWSTR pwszLittleEndian, UINT cch, BYTE *pbBigEndian)
{
    for (UINT i = cch; i > 0; i--)
    {
        *pbBigEndian++ = static_cast<BYTE> ((*pwszLittleEndian) >> 8);
        *pbBigEndian++ = static_cast<BYTE> ((*pwszLittleEndian++) & 0xFF);
    }
}
