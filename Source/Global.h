// ============================================================================
// FILE: Global.h
//
//      Global include for global definitions, declarations, etc.
//
// Copyright (c) 1999-2000 Microsoft Corporation. All rights reserved.

#pragma once

// File types. Includes an enumeration of filetypes, as well as 
// strings for 

enum AWS_FILETYPE
{
    AWS_FILETYPE_UNKNOWN = -1,              // Unknown type
    AWS_FILETYPE_XML = 0,                   // Static XML file
    AWS_FILETYPE_PASP,                      // Dynamic XML file
    AWS_FILETYPE_END,                       // None after here
};

const LPCSTR g_szFileTypesA[] = 
{
    "XML", "PASP"
};

const char g_szSourceFile[] = "SSXSLSRCFILE:";

extern bool ModuleGlobalInitialize();
extern void ModuleGlobalUninitialize();

// Global, cached BSTRs.
extern BSTR g_bstrServer;
extern BSTR g_bstrBrowserType;

// Use for simple filename/pathname mappings
extern fso::IFileSystem *g_fileSystemObject;

// Global cache for IXMLDOMDocument interface pointers.
extern CXmlCache *g_xmlCache;

enum Xml3Availability {
    xml3AvailabilityUnchecked,
    xml3AvailabilityUnavailable,
    xml3AvailabilityAvailable
};

extern Xml3Availability g_xml3Availability;
