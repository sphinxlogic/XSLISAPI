
// ============================================================================
// FILE: PIParse.h
//
//    Description: Used to parse a ProcessingInstruction into its
//                 requisite elements.
//
// Copyright (c) 1999-2000 Microsoft Corporation.  All rights reserved.


#pragma once

class PIParseInfo
{
  public:
    PIParseInfo();
    ~PIParseInfo();

    HRESULT  Clear();
    wchar_t *Find(wchar_t* name); // NULL if not found
    HRESULT  Parse(wchar_t *pwszPIContents);

  private:
    void SetField(wchar_t* pField, wchar_t* start, wchar_t* end);
    
    struct ATTRIBUTE_INFO
    {
        wchar_t* _pszName;
        wchar_t* _pszValue;
    };
    ATTRIBUTE_INFO* _pAttrs;
    long            _lSize;
    long            _lCount;
    wchar_t*        _pszType;
};
