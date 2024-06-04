//+---------------------------------------------------------------------------
//
//  Copyright (C) Microsoft Corporation, 1999-2000.
//
//  File:       hashtable.h
//
//  Contents:   Defines simple hashtable class for string keys on IUnknown.
//
//----------------------------------------------------------------------------

#pragma once

struct HashEntry {
    char*      m_szKey;
    long       m_lHash;
    IUnknown*  m_pUnk;
    HashEntry* m_pNext;
};
typedef HashEntry* PHashEntry;

class HashTable
{
  public:

    HashTable();
    ~HashTable();

    HRESULT init(long initialSize, double rehashFactor, double growthRate);

    IUnknown* find(char* pszKey);

    // add also replaces existing entries.
    // returns NULL of out of memory.
    bool add(char* pszKey, IUnknown* pUnk);

    long getCount() const { return m_count; }
    long getCapacity() const { return m_capacity; }

    HashEntry* get(long i);

    bool remove(char* pszKey);
    void clear();

  private:
    HashEntry* _get(char* pszKey);
    long Hash(char* pszKey);
    HRESULT rehash();

    PHashEntry* m_pTable;
    long        m_capacity;
    long        m_count;
    double      m_loadFactor;
    double      m_growthRate;
    long        m_threshHold;
};

