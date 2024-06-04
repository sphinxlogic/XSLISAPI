// xslisapi2.cpp : Implementation of DLL Exports.


// Note: Proxy/Stub Information
//      To build a separate proxy/stub DLL, 
//      run nmake -f xslisapi2ps.mk in the project directory.

#include "stdafx.h"
#include "resource.h"
#include <initguid.h>
#include "xslisapi2.h"

#include "xslisapi2_i.c"

#include "XMLServerDoc.h"
#include "ASPPreprocessor.h"

CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
    OBJECT_ENTRY(CLSID_XMLServerDocument, CXMLServerDocument)
    OBJECT_ENTRY(CLSID_ASPPreprocessor, CASPPreprocessor)
END_OBJECT_MAP()

/////////////////////////////////////////////////////////////////////////////
// DLL Entry Point

extern "C"
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
    BOOL retVal = TRUE;
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        _Module.Init(ObjectMap, hInstance, &LIBID_XSLISAPI2Lib);
        DisableThreadLibraryCalls(hInstance);

        retVal = ModuleGlobalInitialize();
        
    } else if (dwReason == DLL_PROCESS_DETACH) {
        
        _Module.Term();
        ModuleGlobalUninitialize();
        
    }
    
    return retVal;    // ok
}

/////////////////////////////////////////////////////////////////////////////
// Used to determine whether the DLL can be unloaded by OLE

STDAPI DllCanUnloadNow(void)
{
    return (_Module.GetLockCount()==0) ? S_OK : S_FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// Returns a class factory to create an object of the requested type

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    return _Module.GetClassObject(rclsid, riid, ppv);
}

/////////////////////////////////////////////////////////////////////////////
// DllRegisterServer - Adds entries to the system registry

STDAPI DllRegisterServer(void)
{
    // registers object, typelib and all interfaces in typelib
    return _Module.RegisterServer(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry

STDAPI DllUnregisterServer(void)
{
    return _Module.UnregisterServer(TRUE);
}


