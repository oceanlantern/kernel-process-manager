// copyright (c) Tim Adams   
// PMKernel
//
// Monitor process launches - notify client on user side and process response
//
// History:
//

#include "stdafx.h"
#include "PMKernel.h"

//
// Global Data
//
bool PMKernel::m_HooksSet = false;
LONG PMKernel::m_HookUseCount = 0;

// =============================================================================
//
// Function : PMKernel
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================
 
PMKernel::PMKernel():
    m_Active(false),
    m_pKEventObject(false),
    m_ClientPid(0),
    m_pBlackList(NULL),
    m_HooksSet(false)
{
    DbgMsg((LOG_DBG, "+%s ", __FUNCTION__));
           
    ExInitializeFastMutex(&m_hBlackListLock);
                                                    
    DbgMsg((LOG_DBG, "-%s ", __FUNCTION__));
}

// =============================================================================
//
// Function : ~PMKernel
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

PMKernel::~PMKernel()
{
    DbgMsg((LOG_DBG, "+%s ", __FUNCTION__));

    DbgMsg((LOG_DBG, "-%s ", __FUNCTION__));
}

// =============================================================================
//
// Function : SetProcessHooks
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void PMKernel::SetProcessHooks()
{
    DbgMsg((LOG_DBG, "+%s", __FUNCTION__));

    if (!m_HooksSet)
    {
        NTSTATUS status = PsSetCreateProcessNotifyRoutine(CreateProcessNotifyEntry, false);

        if (STATUS_SUCCESS == status)
        {
            status = PsSetCreateThreadNotifyRoutine(CreateThreadNotifyEntry);

            if (STATUS_SUCCESS == status)
            {
                m_HooksSet = true;
            }
            else
            {
                PsSetCreateProcessNotifyRoutine(CreateProcessNotifyEntry, true);

                DbgMsg((LOG_ERR, "%s - PsSetCreateThreadNotifyRoutine failed 0x%x", __FUNCTION__, status));
            }
        }
        else
        {
            DbgMsg((LOG_ERR, "%s - PsSetCreateProcessNotifyRoutine failed 0x%x", __FUNCTION__, status));
        }    
    }

    #if USE_LOAD_IMAGE_NOTIFY
    PsSetLoadImageNotifyRoutine(LoadImageNotifyEntry);
    #endif

    DbgMsg((LOG_DBG, "-%s", __FUNCTION__));
}

// =============================================================================
//
// Function : RemoveProcessHooks
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void PMKernel::RemoveProcessHooks()
{
    DbgMsg((LOG_DBG, "+%s", __FUNCTION__));

    #if USE_LOAD_IMAGE_NOTIFY
    PsRemoveLoadImageNotifyRoutine(LoadImageNotifyEntry);
    #endif

    if (m_HooksSet)
    {
        while (m_HookUseCount > 0)
        {
            DbgMsg((LOG_DBG, "%s - Waiting for m_HookUseCount %d", __FUNCTION__, m_HookUseCount));
        }

        PsRemoveCreateThreadNotifyRoutine(CreateThreadNotifyEntry);
        PsSetCreateProcessNotifyRoutine(CreateProcessNotifyEntry, true);
        m_HooksSet = false;
    }

    DbgMsg((LOG_DBG, "-%s", __FUNCTION__));
}

// =============================================================================
//
// Function : Init
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

bool PMKernel::Init(INIT_PL*pinit)
{
    bool result = false;
    PFILE_OBJECT pfo = NULL;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    DbgMsg((LOG_INFO, "+%s ", __FUNCTION__));

    status = ObReferenceObjectByHandle((HANDLE)pinit->hEvent, 0, NULL, UserMode, (void**)&pfo, NULL);
       
    if (NT_SUCCESS(status))
    {
        m_pKEventObject = pfo;
            
        m_ClientPid = (HANDLE)pinit->ClientPid;

        DbgMsg((LOG_DBG, "%s - Referenced user event %p ClientPid %p", __FUNCTION__, m_pKEventObject, m_ClientPid));

        m_Active = true;
       
        result = true;
    }
    else
    {
        DbgMsg((LOG_DBG, "%s - Failed to open user event status: 0x%X", __FUNCTION__, status));    
    }

    DbgMsg((LOG_INFO, "-%s ", __FUNCTION__));

    return result;
    
}

// =============================================================================
//
// Function : Term
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

bool PMKernel::Term()
{
    bool result = false;
    int queueCount = 0;

    DbgMsg((LOG_DBG, "+%s ", __FUNCTION__));

    m_Active = false;

    m_ClientPid = 0;

    queueCount = m_EventQueue.GetCount();
    m_EventQueue.EmptyList();

    if (m_pKEventObject)
    {
        DbgMsg((LOG_DBG, "%s - Dereferencing user event %p", __FUNCTION__, m_pKEventObject));     

        ObDereferenceObject(m_pKEventObject);           
        m_pKEventObject = NULL;
    }

    EmptyProcessQueue();

    CAutoLockerFm lk(&m_hBlackListLock);
    if (m_pBlackList)
    {
        CDelete(m_pBlackList);
        m_pBlackList = NULL;
    }

    DbgMsg((LOG_DBG, "%s - Events 0x%x  Processes 0x%x MemCount 0x%x", __FUNCTION__, m_EventQueue.GetCount(), m_ProcessQueue.GetCount(), GetMemCount()));

    result = true;

    DbgMsg((LOG_DBG, "-%s ", __FUNCTION__));

    return result;
}

// =============================================================================
//
// Function : CreateProcessNotifyExEntry
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void PMKernel::CreateProcessNotifyExEntry (PEPROCESS peprocess, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo)
{
    if (g_pdevice &&
        g_pdevice->m_pl.GetActive())
    {
        g_pdevice->m_pl.CreateProcessNotifyEx(peprocess, ProcessId, CreateInfo);
    }
}

// =============================================================================
//
// Function : CreateProcessNotifyEntry
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void PMKernel::CreateProcessNotifyEntry(HANDLE hParent, HANDLE ProcessId, BOOLEAN Create)
{
    if (g_pdevice)
    {         
        g_pdevice->m_pl.CreateProcessNotify(hParent, ProcessId, Create);
    }
}

// =============================================================================
//
// Function : CreateThreadNotifyEntry
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void PMKernel::CreateThreadNotifyEntry(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create)
{
    if (g_pdevice)
    {
        if (g_pdevice->m_pl.GetActive())
        {
            g_pdevice->m_pl.CreateThreadNotify(ProcessId, ThreadId, Create);
        }
    }
}

// =============================================================================
//
// Function : LoadImageNotifyEntry
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void PMKernel::LoadImageNotifyEntry(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO pimageInfo)
{
    if (g_pdevice &&
        g_pdevice->m_pl.GetActive())
    {
        g_pdevice->m_pl.LoadImageNotify(FullImageName, ProcessId, pimageInfo);
    }
}

// =============================================================================
//
// Function : Ioctl
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

NTSTATUS PMKernel::Ioctl(PIRP Irp)
{
    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);

    Irp->IoStatus.Information = 0;

    if (!m_Active &&
        (irpStack->Parameters.DeviceIoControl.IoControlCode != DrvMsgInitPL))
    {
        // For security - if not initilized and message is not DrvMsgInitPL then skip
        // goto - support one exit from this function to insure correct cleanup
        status = STATUS_INVALID_PARAMETER;
        goto done;        
    }

    switch(irpStack->Parameters.DeviceIoControl.IoControlCode)
    {
        case DrvMsgInitPL:
        {
            DbgMsg((LOG_INFO, "%s - DrvMsgInitPL", __FUNCTION__));         

            if  (irpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(INIT_PL))
            {
                DbgMsg((LOG_ERR, "%s - DrvMsgInitPL - STATUS_BUFFER_TOO_SMALL", __FUNCTION__));

                status = STATUS_BUFFER_TOO_SMALL;
            }
            else
            {
                INIT_PL*pinit = (INIT_PL*)Irp->AssociatedIrp.SystemBuffer;

                if (Init(pinit))
                {
                    DbgMsg((LOG_INFO, "%s - DrvMsgInitPL - succeeded", __FUNCTION__));

                    status = STATUS_SUCCESS;
                }
                else
                {
                    DbgMsg((LOG_ERR, "%s - DrvMsgInitPL - failed", __FUNCTION__));                 

                    status = STATUS_OBJECT_TYPE_MISMATCH;
                }
            }

            break;
        }

        case DrvMsgTermPL:
        {
            DbgMsg((LOG_DBG, "%s - DrvMsgTermPL", __FUNCTION__));         

            if  (irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(TERM_PL))
            {
                DbgMsg((LOG_ERR, "%s - DrvMsgTermPL - STATUS_BUFFER_TOO_SMALL", __FUNCTION__));             

                Irp->IoStatus.Information = 0;
                status = STATUS_BUFFER_TOO_SMALL;
            }
            else
            {
                PTERM_PL pterm = (PTERM_PL)Irp->AssociatedIrp.SystemBuffer;

                Term();

                pterm->MemCount = GetMemCount();

                Irp->IoStatus.Information = sizeof(TERM_PL);
                status = STATUS_SUCCESS;
            }
            
            break;
        }

        case DrvMsgGetNextObj:
        {
            if  (irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(PMEVENT))
            {
                DbgMsg((LOG_ERR, "%s - DrvMsgGetNextPLObj - STATUS_BUFFER_TOO_SMALL", __FUNCTION__));             

                status = STATUS_BUFFER_TOO_SMALL;
            }
            else
            {
                PPMEVENT ppmeventOut = (PPMEVENT)Irp->AssociatedIrp.SystemBuffer;

                Irp->IoStatus.Information = sizeof(PMEVENT);
                status = STATUS_SUCCESS;

                ppmeventOut->m_plObj.m_Result = false;

                PPMEVENT ppmeventQueue = (PPMEVENT)m_EventQueue.RemoveHead();

                if (ppmeventQueue)
                {
                    *ppmeventOut = *ppmeventQueue;
                    ppmeventOut->m_plObj.m_Result = true;
                    
                    DbgMsg((LOG_INFO, "%s - DrvMsgGetNextPLObj - Succeeded for Type 0x%x", __FUNCTION__, ppmeventQueue->m_plObj.m_EventType));                    
                    
                    CDelete(ppmeventQueue);                  
                }
                else
                {     
                    DbgMsg((LOG_INFO, "%s - DrvMsgGetNextPLObj - m_EventQueue empty", __FUNCTION__));
                }
            }

            break;
        }

        case DrvMsgCompleteObj:
        {
            DbgMsg((LOG_INFO, "%s - DrvMsgCompletePLObj", __FUNCTION__));         

            if  (irpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(PMEVENT))
            {
                DbgMsg((LOG_ERR, "%s - DrvMsgCompletePLObj - STATUS_BUFFER_TOO_SMALL", __FUNCTION__));

                status = STATUS_BUFFER_TOO_SMALL;
            }
            else
            {
                PPMEVENT ppmeventIn = (PPMEVENT)Irp->AssociatedIrp.SystemBuffer;

                if (ePLOBJType_ProcessStart == ppmeventIn->m_plObj.m_EventType)
                {
                    PLPROCESS plproc;

                    if (GetProcessDataCopy((HANDLE)ppmeventIn->PlProcess.m_ProcessID, plproc))
                    {
                        DbgMsg((LOG_INFO, "%s - DrvMsgCompletePLObj - PLPROCESS Updated ", __FUNCTION__));                     

                        plproc = ppmeventIn->PlProcess;

                        UpdateProcessData((HANDLE)ppmeventIn->PlProcess.m_ProcessID, plproc);
                    }
                }
                           
                if (ppmeventIn->m_plObj.m_hWaitEvent)
                {
                    LONG prevState = 0;
                   
                    ZwSetEvent((HANDLE)ppmeventIn->m_plObj.m_hWaitEvent, &prevState);
                }

                status = STATUS_SUCCESS;
            }

            break;
        }

        case DrvMsgSetBlackList:
        {
            DbgMsg((LOG_INFO, "%s - DrvMsgSetBlackList", __FUNCTION__));         

            if  (irpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(PROCESS_LIST))
            {
                DbgMsg((LOG_ERR, "%s - DrvMsgSetBlackList - STATUS_BUFFER_TOO_SMALL", __FUNCTION__));             

                status = STATUS_BUFFER_TOO_SMALL;
            }
            else
            {
                PPROCESS_LIST pplist = (PPROCESS_LIST)Irp->AssociatedIrp.SystemBuffer;      

                if (pplist->Size == irpStack->Parameters.DeviceIoControl.InputBufferLength)
                {
                    CAutoLockerFm lk(&m_hBlackListLock);

                    if (m_pBlackList)
                    {
                        CDelete(m_pBlackList);
                        m_pBlackList = NULL;
                    }

                    m_pBlackList = (PPROCESS_LIST)CNew(pplist->Size);

                    if (m_pBlackList)
                    {
                        DbgMsg((LOG_DBG, "%s - DrvMsgSetBlackList - New black list set count 0x%x size 0x%x", __FUNCTION__, pplist->Count, pplist->Size));
                     
                        UserMsg((LOG_DBG, "%s - DrvMsgSetBlackList - New black list set count 0x%x size 0x%x", __FUNCTION__, pplist->Count, pplist->Size));

                         memcpy(m_pBlackList, pplist, pplist->Size);
                         status = STATUS_SUCCESS;
                    }
                    else
                    {
                        DbgMsg((LOG_ERR, "%s - DrvMsgSetBlackList - Failed to alloc buff", __FUNCTION__));
                     
                        UserMsg((LOG_ERR, "%s - DrvMsgSetBlackList - Failed to alloc buff", __FUNCTION__));                    
                    }
                }
                else
                {
                    status = STATUS_BUFFER_TOO_SMALL;
                }
            }

            break;
        }

    }

done:

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

// =============================================================================
//
// Function : IsParentProcess
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

bool PMKernel::IsParentProcess(PWSTR pwzname, HANDLE hparent, HANDLE processId, DWORD64 &parentID)
{
    bool result = false;

    CAutoLockerFm lk(&m_hBlackListLock);

    parentID = (DWORD)-1;

    if (m_pBlackList)
    {            
        DWORD i = 0;

        _wcslwr(pwzname);

        for (i = 0; i < m_pBlackList->Count; i++)
        {
            if (wcsstr(pwzname, m_pBlackList->Entry[i].Path))
            {
                parentID = m_pBlackList->Entry[i].ParentID;

                DbgMsg((LOG_DBG, "%s - Found ParentID 0x%x for process PID 0x%I64x - %ws", __FUNCTION__, parentID, processId, pwzname));

                result = true;
                break;
            }
        }
    }

    if (!result)
    {
        DbgMsg((LOG_INFO, "%s - Did not find entry for process PID 0x%x - %ws", __FUNCTION__, processId, pwzname));
    }

    return result;
}

// =============================================================================
//
// Function : IsChildProcess
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

bool PMKernel::IsChildProcess(PWSTR pwzname, HANDLE hparent, HANDLE processId, DWORD64 &parentID)
{
    bool result = false;
    PLPROCESS plproc = {0};

    parentID = (DWORD)-1;

    if (hparent &&
       (GetProcessDataCopy(hparent, plproc)))
    {
        CProcessName processNameParent(hparent);

        parentID = plproc.m_ParentID;

        DbgMsg((LOG_DBG, "%s - %ws is child process of parent PID 0x%x ParentID 0x%I64x - %ws", __FUNCTION__, pwzname, plproc.m_ProcessID, parentID, processNameParent.GetPath()));

        result = true;
    }
    else
    {
        DbgMsg((LOG_INFO, "%s - No parent for process PID 0x%x - %ws", __FUNCTION__, processId, pwzname));
    }

    return result;
}

// =============================================================================
//
// Function : CreateProcessNotifyEx
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void PMKernel::CreateProcessNotifyEx (PEPROCESS perocess, HANDLE processId, PPS_CREATE_NOTIFY_INFO pcreateInfo)
{
    if (pcreateInfo)
    {
        if (pcreateInfo->FileOpenNameAvailable)
        {       
            DbgMsg((LOG_INFO, "%s - process PID 0x%x CreateTID 0x%x PFO %p - Image: %wZ  CmdLine: %wZ", __FUNCTION__, processId, pcreateInfo->CreatingThreadId, pcreateInfo->FileObject, pcreateInfo->ImageFileName, pcreateInfo->CommandLine));

            CreateProcessNotify(pcreateInfo->ParentProcessId, processId, true);    
        }
    }
    else
    {
        CreateProcessNotify(0, processId, false);    
    }
}

// =============================================================================
//
// Function : CreateProcessNotify
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void PMKernel::CreateProcessNotify(HANDLE hparent, HANDLE processId, BOOLEAN create)
{
    InterlockedIncrement(&m_HookUseCount);
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    CProcessName processName(processId);

    if (m_Active)
    {
       if (IsInteractiveProcess (processId))
       {  
           SendInfoProcessStart(processName.GetPath(), hparent, processId, create?true:false);               

           if (create)
           {
                UserMsg((LOG_DBG, "%s - create process PID 0x%x - (%ws) %ws", __FUNCTION__, processId, processName.GetName(), processName.GetPath()));

                if (processName.GetAllocd())
                {   
                    DWORD64 parentID = (DWORD)-1;
                    BOOLEAN isParent = false;
                    BOOLEAN isChild = false;

                    isParent = IsParentProcess(processName.GetPath(), hparent, processId, parentID);

                    if (!isParent)
                    {
                        isChild = IsChildProcess(processName.GetPath(), hparent, processId, parentID);
                    }

                    UserMsg((LOG_DBG, "%s - ++++++ process is interactive PID 0x%x - (%ws) %ws ", __FUNCTION__, processId, processName.GetName(), processName.GetPath()));

                    if (isParent || 
                        isChild)
                    {
                        PLPROCESS*plzproc = NULL;

                        UserMsg((LOG_DBG, "%s - ++++++ process PID 0x%x is parent %d or child %d ParentID 0x%I64x - (%ws) %ws ", __FUNCTION__, processId, isParent, isChild, parentID, processName.GetName(), processName.GetPath()));

                        pplproc = (PLPROCESS*)CNew(sizeof(PLPROCESS));

                        if (pplproc)
                        {
                            PKLIST_NODE pnode = NULL;

                            //
                            // Init process struct
                            //
                            pplproc->m_ProcessID = (DWORD64)processId;
                            pplproc->m_ParentID = parentID;
               
                            pnode = m_ProcessQueue.AddTail(pplproc);

                            if (pnode)
                            {
                                UserMsg((LOG_DBG, "%s - Process added PID 0x%x Count %d - (%ws) %ws", __FUNCTION__, processId, m_ProcessQueue.GetCount(), processName.GetName(), processName.GetPath()));
                                DbgMsg((LOG_DBG, "%s - Process added PID 0x%x Count %d - (%ws) %ws", __FUNCTION__, processId, m_ProcessQueue.GetCount(), processName.GetName(), processName.GetPath()));
                            }
                            else
                            {
                                UserMsg((LOG_ERR, "%s - Failed to add PID 0x%x - (%ws) %ws", __FUNCTION__, processId, processName.GetName(), processName.GetPath()));

                                CDelete(pplproc);

                                pplproc = NULL;
                            }
                        }
                        else
                        {
                            UserMsg((LOG_ERR, "%s - Failed to alloc memory for PID 0x%x - (%ws) %ws", __FUNCTION__, processId, processName.GetName(), processName.GetPath()));
                        }
                    }
                    else
                    {
                        UserMsg((LOG_DBG, "%s ------- skipping process PID 0x%x is not parent or child - (%ws) %ws", __FUNCTION__, processId, processName.GetName(), processName.GetPath()));   
                    }
                }
                else
                {
                    UserMsg((LOG_DBG, "%s ------ skipping process PID 0x%x does not have name - (%ws) %ws", __FUNCTION__, processId, processName.GetName(), processName.GetPath()));
                }
            }
            else
            {
               PLPROCESS plproc = {0};

                //
                // Remove process from list
                //
                if (GetProcessDataCopy(processId, plproc, true))
                {
                    UserMsg((LOG_DBG, "%s - Deleted process PID 0x%x Count %d - (%ws) %ws", __FUNCTION__, processId, m_ProcessQueue.GetCount(), processName.GetName(), processName.GetPath()));
                    DbgMsg((LOG_DBG, "%s - Deleted process PID 0x%x Count %d - (%ws) %ws", __FUNCTION__, processId, m_ProcessQueue.GetCount(), processName.GetName(), processName.GetPath()));
                }
                else
                {
                    UserMsg((LOG_DBG, "%s - Delete process did not find process PID 0x%x Count %d - (%ws) %ws", __FUNCTION__, processId, m_ProcessQueue.GetCount(), processName.GetName(), processName.GetPath()));
                    DbgMsg((LOG_DBG, "%s - Delete process did not find process PID 0x%x Count %d - (%ws) %ws", __FUNCTION__, processId, m_ProcessQueue.GetCount(), processName.GetName(), processName.GetPath()));
                }
            }
        }
        else
        {
            UserMsg((LOG_DBG, "%s ------- skipping process PID 0x%x is not Interactive - (%ws) %ws", __FUNCTION__, processId, processName.GetName(), processName.GetPath()));   
        }
    }
    else
    {
        if (create)
        {
            DbgMsg((LOG_DBG, "%s - PL Inactive Create process PID 0x%x - (%ws) %ws", __FUNCTION__, processId, processName.GetName(), processName.GetPath()));
        }
        else
        {
            DbgMsg((LOG_DBG, "%s - PL Inactive Delete process PID 0x%x - (%ws) %ws", __FUNCTION__, processId, processName.GetName(), processName.GetPath()));
        }
    }

    InterlockedDecrement(&m_HookUseCount);
}

// =============================================================================
//
// Function : CreateThreadNotify
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void PMKernel::CreateThreadNotify(HANDLE processId, HANDLE threadId, BOOLEAN create)
{
    InterlockedIncrement(&m_HookUseCount);

    DbgMsg((LOG_INFO, "+%s ", __FUNCTION__));

    if (create)
    {
        PLPROCESS plproc = {0};

        if (GetProcessDataCopy(processId, plproc))
        {
            if (!plproc.m_FirstThread)
            {   
                CProcessName processName(processId);

                UserMsg((LOG_DBG, "%s - First thread for process PID 0x%x ParentID 0x%I64x - (%ws) %ws", __FUNCTION__, processId, plproc.m_ParentID, processName.GetName(), processName.GetPath()));

                plproc.m_FirstThread = true;

                UpdateProcessData(processId, plproc);

                SendProcessStart(processName.GetPath(), &plproc, processId, threadId);
            }
        }
                            
    }

    DbgMsg((LOG_INFO, "-%s ", __FUNCTION__));

    InterlockedDecrement(&m_HookUseCount);
}

// =============================================================================
//
// Function : LoadImageNotify
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void PMKernel::LoadImageNotify(PUNICODE_STRING pfullImageName, HANDLE processId, PIMAGE_INFO pimageInfo)
{
    PLPROCESS plproc = {0};

    DbgMsg((LOG_INFO, "+%s ", __FUNCTION__));
   
    if (GetProcessDataCopy(processId, plproc))
    {
        CProcessName processName(processId);

        DbgMsg((LOG_INFO, "%s - Loading image 64BitOS 0x%x 64BitProcess 0x%x for process PID 0x%x - (%ws) Image %wZ", __FUNCTION__, plproc.m_Is64BitOS, plproc.m_Is64BitProcess, processId, processName.GetName(), pfullImageName));

        if (!plproc.m_HaveKernel32)
        {        
            PWSTR pwzimageName = UniToWide(pfullImageName);

            if (pwzimageName)
            {
                PWSTR pkernel32 = NULL;
                const WCHAR wzkernel32_64BitOS_32Bitprocess[] = L"syswow64\\kernel32.dll";
                const WCHAR wzkernel32_32Bitos_32Bitprocess[] = L"system\\kernel32.dll";
                const WCHAR wzkernel32_64Bitos_64Bitprocess[] = L"system\\kernel32.dll";

                _wcslwr(pwzimageName);

                if (plproc.m_Is64BitOS)
                {
                    if (plproc.m_Is64BitProcess)
                    {
                        pkernel32 = wcsstr(pwzimageName, wzkernel32_64Bitos_64Bitprocess);                        
                    }
                    else
                    {
                        pkernel32 = wcsstr(pwzimageName, wzkernel32_64BitOS_32Bitprocess);
                    }
                }
                else
                {
                    pkernel32 = wcsstr(pwzimageName, wzkernel32_32Bitos_32Bitprocess);
                }

                if (pkernel32)
                {
                    plproc.m_HaveKernel32 = true;

                    UserMsg((LOG_INFO, "%s - Have kernel32 PID 0x%x - (%ws) Image %wZ ", __FUNCTION__, processId, processName.GetName(), pfullImageName));    
                }
        
                CDelete(pwzimageName);

            }
        }       
    }

    DbgMsg((LOG_INFO, "-%s ", __FUNCTION__));
}

// =============================================================================
//
// Function : CopyImagePath
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void PMKernel::CopyImagePath(PWSTR pwzname, PPMEVENT ppmevent)
{
    int strlenBytes = 0;
    int maxSize = 0;
    const int cMaxTgtBytes = ((MAX_PATH - 1) * sizeof(WCHAR)); // -1 leave room for null

    memset(ppmevent->PlInfoProcess.m_Path, 0, sizeof(ppmevent->PlInfoProcess.m_Path));

    strlenBytes = (int)(wcslen(pwzname) * sizeof(WCHAR));
    maxSize = (strlenBytes < cMaxTgtBytes) ? strlenBytes : cMaxTgtBytes; 
    memcpy(ppmevent->PlInfoProcess.m_Path, pwzname, maxSize);
}

// =============================================================================
//
// Function : SendInfoProcessStart
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

bool PMKernel::SendInfoProcessStart(PWSTR pwzname, HANDLE parentPID, HANDLE processId, bool create)
{
    bool result = false;

    if (m_Active &&
        pwzname)
    {
        bool attached = false;
        KAPC_STATE apcState;

        PPMEVENT ppmevent = (PPMEVENT)CNew(sizeof(PLEVENT));

        if (ppmevent)
        {   
            //
            // Attach to client process and open target process handle in its context
            // Queue an event for client so it can process 
            //
            if (AttachtoProcess(m_ClientPid, &attached, &apcState))
            {
                ppmevent->PlInfoProcess.m_hProcess = (DWORD64)OpenProcess(processId, MAXIMUM_ALLOWED);
                DetachfromProcess(attached, &apcState);

                ppmevent->PlInfoProcess.m_Create = create;
                ppmevent->PlInfoProcess.m_ParentPID = (DWORD64)parentPID;
                ppmevent->PlInfoProcess.m_ProcessID = (DWORD64)processId;

                CopyImagePath(pwzname, ppmevent);

                ppmevent->m_plObj.m_EventType = ePLOBJType_InfoProcess;
            
                result = QueueUserEvent(ppmevent);
            }
            else
            {
                DbgMsg((LOG_ERR, "%s - Skipping QueueUserEvent could not attach to Overlay process PID 0x%x - %ws", __FUNCTION__, processId, pwzname));
            }

            if (!result)
            {
                CDelete(ppmevent);
            }
        }
    }            

    return result;
}

// =============================================================================
//
// Function : SendProcessStart
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

bool PMKernel::SendProcessStart(PWSTR pwzname, PLPROCESS*pplproc, HANDLE processId, HANDLE threadId)
{
    bool result = false;
    bool attached = false;
    KAPC_STATE apcState;

    PPMEVENT ppmevent = (PPMEVENT)CNew(sizeof(PLEVENT));

    if (m_Active &&
        ppmevent)
    {   
        pplproc->m_hThreadID = (DWORD64)threadId;
        
        ppmevent->PLPROCESS = *pplproc;

        if (AttachtoProcess(processId, &attached, &apcState))
        {
            DetachfromProcess(attached, &apcState);

            //
            // Open target process in client's context
            //
            if (AttachtoProcess(m_ClientPid, &attached, &apcState))
            {
                ppmevent->PlProcess.m_hProcess = (DWORD64)OpenProcess(processId, MAXIMUM_ALLOWED);
                DetachfromProcess(attached, &apcState);

                if (!ppmevent->PlProcess.m_hProcess)
                {
                    DbgMsg((LOG_ERR, "%s - Skipping Process - Could not open process 0x%I64x or  PID %p - %ws", __FUNCTION__, (HANDLE)ppmevent->PlProcess.m_hProcess, processId, pwzname));
                }
                else
                {
                    //
                    // Init the rest 
                    //
                    ppmevent->m_plObj.m_EventType = ePLOBJType_ProcessStart;
            
                    if (CreateEvent((HANDLE*)& ppmevent->m_plObj.m_hWaitEvent))
                    {
                        ppmevent->m_plObj.m_NeedComplete = true;

                        CopyImagePath(pwzname, ppmevent);

                        result = QueueUserEvent(ppmevent);

                        if (!result)
                        {
                            DbgMsg((LOG_ERR, "%s - QueueUserEvent failed PID 0x%x - %ws", __FUNCTION__, processId, pwzname));

                            ZwClose((HANDLE)ppmevent->m_plObj.m_hWaitEvent);
                        }
                    }
                    else
                    {
                        DbgMsg((LOG_ERR, "%s - Skipping QueueUserEvent could not create wait event PID 0x%x - %ws", __FUNCTION__, processId, pwzname));
                    }
                }
            }
            else
            {
                DbgMsg((LOG_ERR, "%s - Skipping QueueUserEvent could not attach to ClientPid PID 0x%x - %ws", __FUNCTION__, m_ClientPid, pwzname));
            }

        }
        else
        {
            DbgMsg((LOG_ERR, "%s - Skipping QueueUserEvent could not attach to Target process PID 0x%x - %ws", __FUNCTION__, processId, pwzname));
        }

        if (!result)
        {
            CDelete(ppmevent);
        }
    }

    return result;
}

// =============================================================================
//
// Function : GetProcessDataCopy
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

bool PMKernel::GetProcessDataCopy(HANDLE pid, PLPROCESS&plprocess,  bool remove /*false*/)
{
    bool result = false;
        
    TRY
    {
        KIRQL oldirql = 0;
        PLIST_ENTRY pnext = NULL;
        PLIST_ENTRY phead = NULL; 
                
        m_ProcessQueue.Enter(&oldirql);
        
        phead = m_ProcessQueue.GetQueueHead();
        pnext = phead->Flink;

        while (pnext != phead)
        {
            PKLIST_NODE pcurrent = (PKLIST_NODE)pnext;
            pnext = pnext->Flink;

            PPLPROCESS pplproc = (PPLPROCESS)pcurrent->m_pData;

            if (pplproc &&
                ((HANDLE)pplproc->m_ProcessID == pid))
            {
                plprocess = *pplproc;

                result = true;
                
                if (remove)
                {
                    void *pdata = m_ProcessQueue.RemoveNode(pcurrent, false, false);

                    if (pdata)
                    {
                        CDelete(pdata);
                    }
                }    
                
                break;
            }
        } 
        
        m_ProcessQueue.Leave(oldirql);
    }
    CATCH
    {
        ExceptionHandler(__FUNCTION__, GetExceptionCode());
    }
    
    return result;
}

// =============================================================================
//
// Function : UpdateProcessData
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

bool PMKernel::UpdateProcessData(HANDLE pid, PLPROCESS&plprocess)
{
    bool result = false;
        
    TRY
    {
        KIRQL oldirql = 0;
        PLIST_ENTRY pnext = NULL;
        PLIST_ENTRY phead = NULL; 
                
        m_ProcessQueue.Enter(&oldirql);
        
        phead = m_ProcessQueue.GetQueueHead();
        pnext = phead->Flink;

        while (pnext != phead)
        {
            PKLIST_NODE pcurrent = (PKLIST_NODE)pnext;
            pnext = pnext->Flink;

            PPLPROCESS pplproc = (PPLPROCESS)pcurrent->m_pData;

            if (pplproc &&
                ((HANDLE)pplproc->m_ProcessID == pid))
            {
                *pplproc = plprocess;

                result = true;
                
                break;
            }
        } 
        
        m_ProcessQueue.Leave(oldirql);
    }
    CATCH
    {
        ExceptionHandler(__FUNCTION__, GetExceptionCode());
    }
    
    return result;
}

// =============================================================================
//
// Function : EmptyProcessQueue
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
//=============================================================================

void PMKernel::EmptyProcessQueue()
{
    TRY
    {
        KIRQL oldirql = 0;
        void *pdata = NULL;

        DbgMsg((LOG_DBG, "%s - Start Count %d ", __FUNCTION__, m_ProcessQueue.GetCount()));
                
        m_ProcessQueue.Enter(&oldirql);

        m_ProcessQueue.EmptyListFreeDataNoLock();
               
        m_ProcessQueue.Leave(oldirql);

        DbgMsg((LOG_DBG, "%s - Done Count %d ", __FUNCTION__, m_ProcessQueue.GetCount()));
    }
    CATCH
    {
        ExceptionHandler(__FUNCTION__, GetExceptionCode());
    }
}

// =============================================================================
//
// Function : QueueUserEvent
//
// Input  : None 
// Output : None
// Return : None
//
// Description :
//
// If event is queue'd then it cannot be accessed by caller on return
//
//=============================================================================

bool PMKernel::QueueUserEvent(PPMEVENT ppmevent)
{
    bool result = false;
    int eventCount = m_EventQueue.GetCount();

    DbgMsg((LOG_INFO, "+%s ", __FUNCTION__));

    if (m_Active)
    {
        NTSTATUS status = STATUS_UNSUCCESSFUL;
        PKLIST_NODE pnode = NULL;

        HANDLE heventWait = (HANDLE)ppmevent->m_plObj.m_hWaitEvent;

        pnode = m_EventQueue.AddTail(ppmevent);

        if (pnode)
        {
            if (m_pKEventObject)
            {
                LONG previousState = 0;

                //
                // Once in list then function is successful and ppmevent can no longer be accessed                      
                //
                result = true;

                DbgMsg((LOG_DBG, "%s - Queued Event Type 0x%x Count 0x%x and signaled user - status 0x%x", __FUNCTION__, ppmevent->m_plObj.m_EventType, m_EventQueue.GetCount(), status));

                //
                // Signal user thread
                //
                status = KeSetEvent((PKEVENT)m_pKEventObject, 0, false);

                //
                // Can't access ppmevent after ZwSetEvent
                //
                ppmevent = NULL;

                if (heventWait)
                {
                    while (true)
                    {
                        LARGE_INTEGER to;
                        int milliSeconds = 10000;

                        to.QuadPart = -(milliSeconds * 10000);

                        DbgMsg((LOG_DBG, "%s - Starting Wait on completion event", __FUNCTION__));

                        status = ZwWaitForSingleObject(heventWait, TRUE, &to);

                        if (STATUS_SUCCESS == status)
                        {
                            DbgMsg((LOG_DBG, "%s - Wait completed", __FUNCTION__));

                            break;
                        }
                        else
                        {
                            if (STATUS_TIMEOUT == status)
                            {
                                DbgMsg((LOG_DBG, "%s - Timeout Waiting", __FUNCTION__));

                                break;

                            }
                            else
                            {
                                DbgMsg((LOG_DBG, "%s - Wait failed status 0x%x", __FUNCTION__, status));

                                break;
                            }
                        }
                    }

                    ZwClose(heventWait);
                }
            }
            else
            {
                DbgMsg((LOG_ERR, "%s - Invalid event 0x%p", __FUNCTION__, m_pKEventObject));
            }
        }
        else
        {
            DbgMsg((LOG_ERR, "%s - Failed to add node", __FUNCTION__));
        }
    }

    DbgMsg((LOG_INFO, "-%s ", __FUNCTION__));

    return result;
}




