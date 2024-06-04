//+---------------------------------------------------------------------------
//
//  Copyright (C) Microsoft Corporation, 1999-2000.
//
//  File:       hashtable.cpp
//
//  Contents:   Implementation of simple hashtable class for string
//              keys on IUnknown. 
//
//----------------------------------------------------------------------------

#include "StdAfx.h"
#include "hashtable.h"

#define INITIAL_SIZE 17

HashTable::HashTable()
{
    m_count = 0;
	m_capacity = 0;
}

HashTable::~HashTable()
{
    clear();
    delete m_pTable;
}

void
HashTable::clear()
{
    for (long i = 0; i < m_capacity; i++)
    {
        HashEntry* next = m_pTable[i];
        while (next)
        {
            HashEntry* e = next;
            next = next->m_pNext;
            delete e->m_szKey;
            SAFERELEASE(e->m_pUnk);
            delete e;
        }
        m_pTable[i] = 0;
    }
    m_count = 0;
}

HRESULT 
HashTable::init(long initialSize, double loadFactor, double growthRate)
{
    if (growthRate <= 1) return E_INVALIDARG;
    if (loadFactor > 1) return E_INVALIDARG;

    clear();
    m_pTable = new PHashEntry[initialSize];
    if (! m_pTable) return E_OUTOFMEMORY;
    ::memset(m_pTable, 0, sizeof(PHashEntry)*initialSize);
    m_capacity = initialSize;
    m_loadFactor = loadFactor;
    m_growthRate = growthRate;
    m_threshHold = (long)(m_capacity * m_loadFactor);
    return S_OK;
}

long 
HashTable::Hash(char* pszKey)
{
    int result = 0;
    for (char* ptr = pszKey; *ptr; ptr++)
    {
        result = result * 113 + (*ptr);
    }

    return result & 0x7FFFFFFF;
}

IUnknown*
HashTable::find(char* pszKey)
{
    IUnknown* pUnk = NULL;
    HashEntry*e = _get(pszKey);
    if (e) {
        pUnk = e->m_pUnk; 
        pUnk->AddRef();
    }
    return pUnk;
}

HashEntry*
HashTable::_get(char* pszKey)
{
    long hash = Hash(pszKey);
    long i = hash % m_capacity;
    HashEntry* e = m_pTable[i];
    if (e == NULL)
        return NULL;

    // walk the buckets.
    while (e)
    {
        if (hash == e->m_lHash && strcmp(pszKey, e->m_szKey) == 0)
        {
            return e;
        }
        else
            e = e->m_pNext;    
    }
    return NULL;
}

bool
HashTable::add(char* pszKey, IUnknown* pUnk)
{
    if (m_count > m_threshHold) 
    {
        if (FAILED(rehash()))
            return false;
    }

    HashEntry* e = _get(pszKey);
    if (e)
    {
        // found existing key, so replace it.
        SAFERELEASE(e->m_pUnk);
        e->m_pUnk = pUnk;
        SAFEADDREF(pUnk);
        return true;
    }

    // pop new entry into front of list then.
    long hash = Hash(pszKey);
    long i = hash % m_capacity;

    e = new HashEntry;
    if (! e) return NULL;

    e->m_szKey = new char[strlen(pszKey)+1];
    if (!e->m_szKey)
    {
        delete e;
        return false;
    }
    strcpy(e->m_szKey, pszKey);

    e->m_lHash = hash;
    e->m_pUnk = pUnk;
    SAFEADDREF(pUnk);
    e->m_pNext = m_pTable[i];
    m_pTable[i] = e;
    m_count++;

    return true;
}

// rehash
HRESULT
HashTable::rehash()
{
    long newSize = (long)(m_capacity * m_growthRate) + INITIAL_SIZE;
    PHashEntry* newTable = new PHashEntry[newSize];
    if (! newTable) return E_OUTOFMEMORY;
    ::memset(newTable, 0, sizeof(PHashEntry)*newSize);
    
    for (long i = 0; i < m_capacity; i++)
    {
        HashEntry* next = m_pTable[i];
        while (next)
        {
            HashEntry* after = next->m_pNext;
            next->m_pNext = NULL; // de-chain from old bucket list.

            long hash = next->m_lHash;
            long i = hash % newSize;
            HashEntry* e = newTable[i];
            if (! e)
            {
                // start new bucket list.
                newTable[i] = next;
            }
            else
            {
                // chain into new bucket list.
                next->m_pNext = newTable[i];
                newTable[i] = next;
            }
            next = after;
        }
    }

    delete[] m_pTable;
    m_pTable = newTable;
    m_capacity = newSize;
    m_threshHold = (long)(m_capacity * m_loadFactor);
    return S_OK;
}

HashEntry* 
HashTable::get(long i)
{
    return m_pTable[i];
}

bool 
HashTable::remove(char* pszKey)
{
    HashEntry* e = _get(pszKey);
    if (! e) return false;

    long i = e->m_lHash % m_capacity;
    HashEntry* f = m_pTable[i];
    HashEntry* last = NULL;
    while (f && f != e)
    {
        last = f;
        f = f->m_pNext;
    }
    if (f == e)
    {
        if (! last)
        {
            // it was the first;
            m_pTable[i] = f->m_pNext;
        }
        else
        {
            last->m_pNext = f->m_pNext;
        }
        delete e->m_szKey;
        SAFERELEASE(e->m_pUnk);
        delete e;
        m_count--;
        // bugbug - should also think about shrinking table at this point...
        return true;
    }
    return false;
}
