
// ============================================================================
// FILE: PIParse.cpp
//
//    Description: Used to parse a ProcessingInstruction into its
//                 requisite elements.
//
// Copyright (c) 1999-2000 Microsoft Corporation.  All rights reserved.

#include "StdAfx.h"
#include "PIParse.h"

//----------------------------------------------------------------------------
// Helper functions for parsing out the xml-stylesheet PI tags
//----------------------------------------------------------------------------
wchar_t* SkipWhitespace(wchar_t* ptr)
{
    while (*ptr != L'\0' && (*ptr == L' ' || *ptr == L'\t' || *ptr == L'\n' || *ptr == L'\r'))
        ptr++;
    return ptr;
}

wchar_t* SkipName(wchar_t* ptr)
{
    while (*ptr != L'\0' && *ptr != L' ' && *ptr != L'\t' &&
           *ptr != L'\n' && *ptr != L'\r' && *ptr != L'=')
        ptr++;
    return ptr;
}

PIParseInfo::PIParseInfo()
{
    _pAttrs = 0;
    _lSize = _lCount = 0;
}

PIParseInfo::~PIParseInfo()
{
    Clear();
}


HRESULT
PIParseInfo::Clear()
{
    for (long i = 0; i < _lCount; i++) {
        delete _pAttrs[i]._pszName;
        delete _pAttrs[i]._pszValue;
    }
    if (_pAttrs) {
        delete [] _pAttrs;
        _pAttrs = 0;
    }
    _lSize = _lCount = 0;

    return S_OK;
}


void
PIParseInfo::SetField(wchar_t* pField, wchar_t* start, wchar_t* end)
{
    int len = (end - start);
    wchar_t* result = new wchar_t[len+1];
    memcpy(result,start,len*sizeof(wchar_t));
    result[len] = L'\0';

    if (wcscmp(pField, L"type") == 0)
      {   
          _pszType = result;
      }
    if (_lCount == _lSize)
      {
          long newsize = (_lSize*2)+5;
          ATTRIBUTE_INFO* newinfo = new ATTRIBUTE_INFO[newsize];
          ::memset(newinfo,0,sizeof(ATTRIBUTE_INFO)*newsize);
          if (_lCount > 0)
              ::memcpy(newinfo,_pAttrs,sizeof(ATTRIBUTE_INFO)*_lCount);

          if (_pAttrs) {
              delete [] _pAttrs;
          }
          
          _pAttrs = newinfo;
          _lSize = newsize;
      }
    _pAttrs[_lCount]._pszName = pField;
    _pAttrs[_lCount]._pszValue = result;
    _lCount++;
}

wchar_t *
PIParseInfo::Find(wchar_t* name)
{
    if (name) {
        for (long i = 0; i < _lCount; i++) {
            if (wcscmp(_pAttrs[i]._pszName, name) == 0) {
                return _pAttrs[i]._pszValue;
            }
        }
    }
    return NULL;
}


HRESULT
PIParseInfo::Parse(wchar_t *pwszPIContents)
{
    HRESULT hr;
    
    wchar_t* ptr = SkipWhitespace(pwszPIContents);
    while (*ptr) {
        wchar_t* start = ptr;
        ptr = SkipName(start);
        ERRCHECK(!*ptr, E_FAIL);

        int len = ptr - start;
        wchar_t* name = new wchar_t[len+1];
        ERRCHECK(name == NULL, E_OUTOFMEMORY);
        
        ::memcpy(name,start,len*sizeof(wchar_t));
        name[len] = L'\0';

        ptr = SkipWhitespace(ptr);
        if (*ptr != '=') {
            delete[] name;
            RETURNERR(E_FAIL);
        }

        ptr = SkipWhitespace(ptr+1);

        if (*ptr != '\'' && *ptr != '"') {
            delete[] name;
            RETURNERR(E_FAIL);
        }

        wchar_t quote = *ptr;
        ptr++;
        wchar_t* value = ptr;
        while (*ptr && *ptr != quote) {
            ptr++;
        }

        if (*ptr != quote) {
            delete[] name;
            RETURNERR(E_FAIL);
        }

        // Ok, so now we have the name and value of the attribute.
        this->SetField(name, value, ptr);

        ptr = SkipWhitespace(ptr+1);
    }

    hr = S_OK;
  Error:
    return hr;
}
