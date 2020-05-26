// copyright (c) Tim Adams   

#pragma once

#include "kutils.h"

typedef struct _KLIST_NODE
{
    LIST_ENTRY  m_ListEntry;
    PVOID       m_pData;
} KLIST_NODE, *PKLIST_NODE;
    
//
// Forward Declarations
//
typedef class DrvQueue *PDrvQueue;

class DrvQueue
{
    public:

    // Prototypes

    PLIST_ENTRY GetQueueHead(void){return &m_List;}

// =============================================================================
//
// Function : DrvQueue
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

DrvQueue::DrvQueue()
{
    KeInitializeSpinLock(&m_SpinLock);
    InitializeListHead(&m_List);
    m_Count = 0;
}

// =============================================================================
//
// Function : ~DrvQueue
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

DrvQueue::~DrvQueue()
{
    if (m_Count)
    {
        DbgMsg((LOG_ERR, "DrvQueue::~DrvQueue: List being destroyed with nodes 0x%x 0x%x", m_Count, IsListEmpty(&m_List))); 
    }
}

// =============================================================================
//
// Function : AddTail
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

PKLIST_NODE DrvQueue::AddTail(PVOID pdata, bool lock = true)
{
    PKLIST_NODE pnode = NULL;

    KIRQL oldirql = 0;

    if (lock)
    {
        Enter(&oldirql);
    }

    TRY
    {
        pnode = (PKLIST_NODE)CNew(sizeof(KLIST_NODE));

        if (pnode)
        {
            memset(pnode, 0, sizeof(KLIST_NODE));

            pnode->m_pData = pdata;

            InsertTailList(&m_List, (PLIST_ENTRY)pnode);
            ++m_Count;
        }
        else
        {
            DbgMsg((LOG_ERR, "DrvQueue::AddTail: Failed")); 
        }
    }
    CATCH
    {
        ExceptionHandler("DrvQueue::AddTail", GetExceptionCode());
    }

    if (lock)
    {
        Leave(oldirql);
    }

    return pnode;

}

// =============================================================================
//
// Function : RemoveHead
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

PVOID DrvQueue::RemoveHead(bool lock = true)
{
    PVOID pdata = NULL;
    KIRQL oldirql;

    if (lock)
    {
        Enter(&oldirql);
    }

    TRY
    {
        if (!IsListEmpty(&m_List))
        {
            PKLIST_NODE pnode = (PKLIST_NODE)RemoveHeadList(&m_List);
           
            if (pnode)
            {
                pdata = pnode->m_pData;

                --m_Count;

                CDelete(pnode);
            }
        }
    }
    CATCH
    {
        ExceptionHandler("DrvQueue::RemoveHead", GetExceptionCode());
    }

    if (lock)
    {
        Leave(oldirql);
    }

    return pdata;
}

// =============================================================================
//
// Function : RemoveNode
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void *RemoveNode(PKLIST_NODE pnode, bool deleteData = false, bool lock = true)
{
    KIRQL oldirql = 0;
    void *presult = NULL;

    if (lock)
        Enter(&oldirql);

    TRY
    {
        presult = pnode->m_pData;

        RemoveEntryList((PLIST_ENTRY)pnode);

        if (deleteData &&
            pnode->m_pData)
        {
            CDelete(pnode->m_pData);
            presult = NULL;
        }

        --m_Count;
        CDelete (pnode);
    }
    CATCH
    {
        ExceptionHandler("DrvQueue::RemoveNode", GetExceptionCode());        
    }

    if (lock)
        Leave(oldirql);

    return presult;
}

// =============================================================================
//
// Function : Reset
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void Reset(void)
{
    PKLIST_NODE pnode = NULL;

    if (m_Count)
    {
        DbgMsg((LOG_ERR, "DrvQueue::Reset: List being emptied with nodes", m_Count)); 
    }

    EmptyList();
}

// =============================================================================
//
// Function : EmptyList
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void EmptyList(void)
{
    void *pdata = NULL;

    while ((pdata = (void*)RemoveHead()))
    {
    }

    m_Count = 0;
}

// =============================================================================
//
// Function : EmptyListFreeData
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void EmptyListFreeData(void)
{
    PKLIST_NODE pnode = NULL;

    void *pdata = NULL;

    while ((pdata = (void*)RemoveHead()))
    {
        CDelete(pdata);
    }

    m_Count = 0;
}

// =============================================================================
//
// Function : EmptyListFreeDataNoLock
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void EmptyListFreeDataNoLock(void)
{
    PKLIST_NODE pnode = NULL;
    void *pdata = NULL;

    while ((pdata = (void*)RemoveHead(false)))
    {
        CDelete(pdata);
    }

    m_Count = 0;
}


// =============================================================================
//
// Function : FindItemData
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

PKLIST_NODE FindItemData(void *pdata)
{
    KIRQL oldirql;
    PKLIST_NODE pnodeResult = NULL;

    Enter(&oldirql);

    TRY
    {
        PLIST_ENTRY pnext = NULL;
        PLIST_ENTRY phead = NULL; 

        phead = GetListHead();
        pnext = phead->Flink;

        while (pnext != phead)
        {                        
            PKLIST_NODE pcurrent = (PKLIST_NODE)pnext;
            pnext = pnext->Flink;

            if (pdata == pcurrent->m_pData)
            {
                pnodeResult = pcurrent;
                break;
            }
        }
    }
    CATCH
    {
        ExceptionHandler("DrvQueue::FindItemData", GetExceptionCode());
    }

    Leave(oldirql);

    return pnodeResult;
}

// =============================================================================
//
// Function : RemoveItemData
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void *RemoveItemData(void *pdata, bool deleteData = false)
{
    void *presult = NULL;

    TRY
    {
        PKLIST_NODE pnode = FindItemData(pdata);

        if (pnode)
        {
            presult = RemoveNode(pnode, deleteData);
        }
    }
    CATCH
    {
        ExceptionHandler("DrvQueue::RemoveItemData", GetExceptionCode());
    }

    return presult;
}

// =============================================================================
//
// Function : GetCount
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

int GetCount(void)
{
    return m_Count;
}

// =============================================================================
//
// Function : Enter
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void Enter(KIRQL *poldirql)
{
    KeAcquireSpinLock(&m_SpinLock, poldirql);
}

// =============================================================================
//
// Function : Leave
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void Leave(KIRQL oldirql)
{
    KeReleaseSpinLock(&m_SpinLock, oldirql);
}

    // Inline

    // Static Prototypes

    // Data

    protected:

    // Prototypes

    // Static Prototypes

    // Data
        KSPIN_LOCK          m_SpinLock;
        LIST_ENTRY          m_List;
        ULONG               m_Count;
};


