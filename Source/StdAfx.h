// stdafx.h : include file for standard system include files,
//      or project specific include files that are used frequently,
//      but are changed infrequently

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define STRICT
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#define _ATL_APARTMENT_THREADED

#include <atlbase.h>
//You may derive a class from CComModule and use it if you want to override
//something, but do not change the name of _Module
extern CComModule _Module;
#include <atlcom.h>

// Imports.

#import "msxml3.tlb" no_implementation named_guids raw_interfaces_only rename_namespace("msxml")
// MSXML3 doesn't include the GUID for the DOMFreeThreadedDocument.
extern "C" const GUID __declspec(selectany) my_CLSID_DOMFreeThreadedDocument =
    {0x2933bf91,0x7b36,0x11d2,{0xb2,0x0e,0x00,0xc0,0x4f,0x98,0x3e,0x60}};

#import "inetsrv/asp.dll" no_implementation named_guids raw_interfaces_only rename_namespace("asp")

// Scripting runtime for the file-system object.
#import "scrrun.dll" no_implementation named_guids raw_interfaces_only rename_namespace("fso")

using namespace msxml;

#include "Resource.h"
#include "xslisapi2.h"
#include "XMLServerDoc.h"
#include "Utils.h"
#include "xmlcache.h"
#include "Global.h"

#include <wininet.h>
#include <activeds.h>
#include <httpfilt.h>
#include <mtx.h>

#pragma warning(push,3)
#pragma warning(disable:4100)
#pragma warning(disable:4530)
#include <map>
#include <string>
#pragma warning(pop)

#ifndef COUNTOF
#define COUNTOF(x) (sizeof(x)/sizeof(x[0]))
#endif

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.
