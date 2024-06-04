// ============================================================================
// FILE: IISFilter.cpp
//
//      XSLISAPI filter for IIS. Manipulates URL requests to the web server
//      for XML and PASP documents
//
// Copyright (c) 1999-2000 Microsoft Corporation. All rights reserved.

#include "StdAfx.h"

#include <initguid.h>
#include <iadmw.h>
#include <iiscnfg.h>

// Identifier string used by filter

const char    g_pszFilterIdentifier[] = "XSLISAPI Server Side XSL Filter";

char g_szEntryPointFiles[AWS_FILETYPE_END][_MAX_PATH];
int  g_nEntryPointPathLen[AWS_FILETYPE_END];
int  g_nEntryPointSuffixLen[AWS_FILETYPE_END];

// ============================================================================
// CreateEntryPointFileSpecs
//      Creates relative pathnames for entry point files. These
//      files (RedirectorXML.asp, RedirectorASP.asp) must reside in
//      the /xslisapi virtual directory.

HRESULT
CreateEntryPointFileSpecs()
{
    for (int filetype = AWS_FILETYPE_XML; 
         filetype < AWS_FILETYPE_END; 
         filetype++)
    {
        g_nEntryPointPathLen[filetype] = 
            wsprintfA (g_szEntryPointFiles[filetype],

                       // The PASP redirector needs to be relative as
                       // it will be in the same directory as the URL
                       // being hit.
                       (filetype == AWS_FILETYPE_PASP ?
                                   "Redirector%s.asp" :
                                   "/xslisapi/Redirector%s.asp"),
                       
                       g_szFileTypesA[filetype]);

        g_nEntryPointSuffixLen[filetype] = strlen(g_szFileTypesA[filetype]);
    }

    return S_OK;
}

// ============================================================================
// CheckVDirForXMLRegistration
//     Given a web directory, see if it has an extension registered
//     for .XML files.  Return this info into pResult.  Use the
//     IMSAdminBase object here rather than Active Directory's ADSI,
//     since ADSI is only available on Win2K/IIS5.  IMSAdminBase will
//     work on NT4/IIS4 as well. 

HRESULT
CheckVDirForXMLRegistration(PHTTP_FILTER_CONTEXT pfc,
                            char *pszVDirCandidate,
                            bool *pResult)
{
    HRESULT                      hr;
    static CComPtr<IMSAdminBase> s_pcomAdminBase;

    *pResult = false;

    if (!s_pcomAdminBase) {
        hr = s_pcomAdminBase.CoCreateInstance(CLSID_MSAdminBase);
        HRCHECK(FAILED(hr));
    }

    // Each site has a different instance ID.  This needs to go into
    // the metabase request path.
    DWORD instID;
    BOOL res;
    res = (*pfc->ServerSupportFunction)(pfc,
                                        SF_REQ_GET_PROPERTY,
                                        &instID,
                                        SF_PROPERTY_INSTANCE_NUM_ID,
                                        0);
    ERRCHECK(!res, E_FAIL);

    wchar_t iisPath[MAX_PATH];
    wsprintf(iisPath,
             L"/LM/W3svc/%d/ROOT%s",
             instID,
             CComBSTR(pszVDirCandidate));

    METADATA_RECORD metadataRecord;
    wchar_t         buf[2000];
    DWORD           requiredDataLen;
    metadataRecord.dwMDIdentifier = MD_SCRIPT_MAPS;
    metadataRecord.dwMDAttributes = METADATA_INHERIT |
                                    METADATA_PARTIAL_PATH;
    metadataRecord.dwMDUserType = IIS_MD_UT_FILE;
    metadataRecord.dwMDDataType = MULTISZ_METADATA;
    metadataRecord.dwMDDataLen = sizeof(buf);
    metadataRecord.pbMDData =
        reinterpret_cast<unsigned char *>(&buf[0]);

    hr = s_pcomAdminBase->GetData(METADATA_MASTER_ROOT_HANDLE,
                                  iisPath,
                                  &metadataRecord,
                                  &requiredDataLen);

    if (FAILED(hr) && ERROR_INSUFFICIENT_BUFFER == HRESULT_CODE(hr)) {

        // Allocate a sufficiently sized buffer and try again. 
        wchar_t *newBuf =
            reinterpret_cast<wchar_t*>(_alloca(requiredDataLen));
        metadataRecord.dwMDDataLen = requiredDataLen;
        metadataRecord.pbMDData = 
            reinterpret_cast<unsigned char *>(newBuf);

        hr = s_pcomAdminBase->GetData(METADATA_MASTER_ROOT_HANDLE,
                                      iisPath,
                                      &metadataRecord,
                                      &requiredDataLen);
    }

    HRCHECK(FAILED(hr));

    // Data returne din buf represents an array of strings.  Each
    // string is NULL-terminated.  The final string has two
    // consecutive NULLs.  Look through these looking for a ".xml,"
    // (case-insensitive), or a "*,".  We look for the comma to be
    // sure we don't pick up, for instance, ".xmlk". 
    wchar_t *ptr;
    ptr = buf;
    for (;;) {
        if ((_wcsnicmp(ptr, L".xml,", 5) == 0) ||
            (_wcsnicmp(ptr, L"*,", 2) == 0)) {
            *pResult = true;
            break;
        } 

        // Look for the NULL character to end the entry, then move
        // on to the next one.
        while (*ptr++ != NULL) {}

        // If this now is null, then there are no more entries.
        if (*ptr == NULL) {
            break;
        }
    }

    hr = S_OK;
  Error:
    return hr;
}

// ============================================================================
// GetParentFolder
//     Given a filename, find the parent directory of that file
//     (excluding the trailing '/').  This assumes that the entire
//     path starts with '/'.
void
GetParentFolder(char *pszFile,      // [in] incoming file spec
                char *pszParentDir) // [in,out] pointer to string
                                    // buffer to fill in
{
    // Search backwards in pszFile for the first occurence of a '/',
    // then copy everything up to that slash into pszParentDir, and
    // null-terminate it.  Assume that this is a path that starts with
    // a '/' 
    ASSERT(pszFile[0] == '/');
    char *p = pszFile + strlen(pszFile) - 1;
    while (*p != L'/') {
        --p;
    }

    int newLen = p - pszFile;
    lstrcpynA(pszParentDir, pszFile, newLen + 1);
}

// ============================================================================
// CheckForRegisteredXMLExtension
//     Given the specified URL, check to see if the directory it's in
//     has a registered extension set up for .XML files.  Look up the
//     directory chain until the first directory with registered
//     extensions is encountered (i.e., until we hit the first IIS
//     "application"). 
HRESULT
CheckForRegisteredXMLExtension(PHTTP_FILTER_CONTEXT pfc,
                               char *requestedURL,
                               bool *pResult)
{
    HRESULT  hr;
    char     szDirectParentFolder[MAX_PATH];

    GetParentFolder(requestedURL,
                    szDirectParentFolder);

    // Maintain a map of visited vdirs to see if we've been to this
    // directory before and, if so, whether XML is a registered
    // extension there. 
    typedef std::map<std::string, bool> StringBoolMap;
    static StringBoolMap g_vdirsVisited;
    
    StringBoolMap::const_iterator iter =
        g_vdirsVisited.find(std::string(szDirectParentFolder));
    if (iter != g_vdirsVisited.end()) {
        *pResult = iter->second;
        RETURNERR(S_OK);
    }

    // Haven't yet looked up this directory.  Do so now.
    hr = CheckVDirForXMLRegistration(pfc,
                                     szDirectParentFolder,
                                     pResult);
    HRCHECK(FAILED(hr));

    // NOTE: We're not checking for possible out-of-memory failures on
    // this operator[] method.  STL isn't really set up for doing that
    // without a good amount more scaffolding around it.
    g_vdirsVisited[std::string(szDirectParentFolder)] = *pResult;

    hr = S_OK;
  Error:
    return hr;
}

// ============================================================================
// GetFilterVersion
//      IIS Filter entry point, to return information about this
//      filter.
//      Returns BOOL indicating success.

extern "C" BOOL WINAPI
GetFilterVersion(
    PHTTP_FILTER_VERSION pVer)              // [in][out] Version information
{
    ASSERT (pVer != NULL);

    // Initialize.

    if (!ModuleGlobalInitialize ())
    {
        return FALSE;
    }

    // Create pathnames for entry point files.

    if (FAILED(CreateEntryPointFileSpecs ()))
    {
        return FALSE;
    }

    // Initialize the structure given to us.
    pVer->dwFilterVersion = HTTP_FILTER_REVISION;
    lstrcpyA (pVer->lpszFilterDesc, g_pszFilterIdentifier);
    pVer->dwFlags = 
        // Register for header preprocessing only.
        SF_NOTIFY_PREPROC_HEADERS |
        // Normal order.
        SF_NOTIFY_ORDER_DEFAULT | 
        // On secure and non-secure ports.
        SF_NOTIFY_SECURE_PORT | SF_NOTIFY_NONSECURE_PORT;

    return TRUE;
}

// ============================================================================
// HttpFilterProc
//      IIS Filter entry point, to handle an individual filtering step.
//      Returns DWORD indicating action required - see <httpfilt.h>.

extern "C" DWORD WINAPI
HttpFilterProc(
    PHTTP_FILTER_CONTEXT pfc,               // [in] Filter context 
    DWORD dwNotificationType,               // [in] Type of notification
    LPVOID pvNotification)                  // [in][out] Notification-specific data
{
    ASSERT (pfc != NULL);
    ASSERT (dwNotificationType == SF_NOTIFY_PREPROC_HEADERS);

    DWORD dwResult = SF_STATUS_REQ_NEXT_NOTIFICATION;

    PHTTP_FILTER_PREPROC_HEADERS pfph = 
        reinterpret_cast<PHTTP_FILTER_PREPROC_HEADERS>(pvNotification);
        
    ASSERT (pfph != NULL);

    // Get the URL from the header. The URL may contain any address,
    // followed by parameters, etc. (either after a ? or #).

    char szURL[INTERNET_MAX_URL_LENGTH];
    DWORD dwSize = COUNTOF(szURL);
    if (!pfph->GetHeader (pfc, "url", szURL, &dwSize)) {
        dwResult = SF_STATUS_REQ_ERROR;
        goto done;
    }

    if (szURL[0] != '\0') {

        // To extract the filetype, we have to parse the URL to find 
        // last '.' in the address portion.  Also keep track of the
        // last forward slash in the address portion.  When we're
        // done, psz will point to the character after the end of the
        // address portion.
        LPSTR psz;
        LPSTR pszDot = NULL;
        LPSTR pszSlash = NULL;
        for (psz = szURL; 
             *psz != '\0' && *psz != '?' && *psz != '#'; 
             psz++) {
            if (*psz == '.') {
                pszDot = psz;
            } else if (*psz == '/') {
                pszSlash = psz;
            }
        }

        // Only continue on three or four letter filetype.
        if (pszDot != NULL &&
            (pszDot + 4 == psz || pszDot + 5 == psz)) {

            int filetype;
            int suffixLen = psz - pszDot - 1;
            bool foundIt = false;
            
            char c[4];
            c[0] = CHAR_TO_UPPER(pszDot[1]);
            c[1] = CHAR_TO_UPPER(pszDot[2]);
            c[2] = CHAR_TO_UPPER(pszDot[3]);
            c[3] = suffixLen == 3 ? '\0' : CHAR_TO_UPPER(pszDot[4]);

            for (filetype = AWS_FILETYPE_XML; 
                 filetype < AWS_FILETYPE_END; 
                 filetype++) {

                if (suffixLen == g_nEntryPointSuffixLen[filetype] &&
                    !memcmp(c, g_szFileTypesA[filetype], suffixLen)) {
                    foundIt = true;
                    break;
                }
            }

            if (foundIt) {

                // First, if we're an XML file, check to see if XML is 
                // registered as an extension in the requested vdir.
                // If so, don't do XSLISAPI processing at all.
                if (filetype == AWS_FILETYPE_XML) {
                    
                    bool bXMLRegistered;
                    HRESULT hr = CheckForRegisteredXMLExtension(pfc,
                                                                szURL,
                                                                &bXMLRegistered);


                    // Bail out if either XML is a registered
                    // extension here, or if the above failed for some
                    // reason (we'll be cautious here).
                    if (FAILED(hr) || bXMLRegistered) {
                        goto done;
                    }
                }
                
                // Save off the old address, as another header field.
                char cOldChar = *psz;
                
                char *pszFirstToReplace;
                
                *psz = '\0';
                if (!pfph->AddHeader (pfc, 
                                      const_cast<LPSTR>(g_szSourceFile), szURL)) {
                    dwResult = SF_STATUS_REQ_ERROR;
                    goto done;
                }
                *psz = cOldChar;

                // For the PASP filetype, we only replace the filename
                // portion, leaving the path intact.  For others, we
                // replace the whole thing.  We only replace the
                // filename for PASP's so that our RedirectorPASP.asp
                // will be invoked in the same directory as the
                // original, thus preserving the correct application
                // and context state.

                if (filetype == AWS_FILETYPE_PASP) {
                    if (pszSlash) {
                        pszFirstToReplace = pszSlash + 1;
                    } else {
                        pszFirstToReplace = szURL;
                    }
                } else {
                    pszFirstToReplace = szURL;
                }
                    
                if (cOldChar != '\0') {

                    // If the URL contains more than just the
                    // address, we have to move that part if the
                    // URL size is changing.
                    int nURLSizeDiff = g_nEntryPointPathLen[filetype] - 
                        (psz - pszFirstToReplace);
                    if (nURLSizeDiff != 0) {
                        MoveMemory (psz + nURLSizeDiff, psz, 
                                    dwSize - (psz - pszFirstToReplace));
                    }
                } else {
                    pszFirstToReplace[g_nEntryPointPathLen[filetype]] = '\0';
                }
                    
                CopyMemory (pszFirstToReplace,
                            g_szEntryPointFiles[filetype], 
                            g_nEntryPointPathLen[filetype]);

                // Change the URL in the header.
                if (!pfph->SetHeader (pfc, "url", szURL)) {
                    dwResult = SF_STATUS_REQ_ERROR;
                }
            }
        }
    }

  done:
    return dwResult;
}

// ============================================================================
// TerminateFilter
//      Called to terminate the filter. We do shutdown here. Note that
//      this entry point is new to IIS 5.0.

extern "C" BOOL WINAPI
TerminateFilter(
    DWORD /*dwFlags*/)                          // [in] Flags - currently 0
{
    ModuleGlobalUninitialize ();

    return TRUE;
}
