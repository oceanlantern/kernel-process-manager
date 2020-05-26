// copyright (c) Tim Adams   
// PMKernel

#pragma once

#include "drvqueue.h"

//
// Types
//
class PMKernel;
typedef class PMKernel* PPMKernel;

typedef struct       
{
    HANDLE  m_ProcessID;
    HANDLE  m_hWaitEvent;
}PROCESS_ENTRY;

class PMKernel
{
        public:

        // Prototypes
        PMKernel();
        virtual ~PMKernel();
        bool Init(INIT_PL*pinit);
        bool Term();
        NTSTATUS Ioctl(PIRP Irp);
        bool GetActive(){return m_Active;};
        bool QueueUserEvent(PPMEVENT ppmevent);
        // Inline

        // Static Prototypes
        static void SetProcessHooks();
        static void RemoveProcessHooks();
        
// Data

        protected:

        // Prototypes
        void LoadImageNotify(PUNICODE_STRING pfullImageName, HANDLE processId, PIMAGE_INFO pimageInfo);
        bool GetProcessDataCopy(HANDLE pid, PLPROCESS &plprocess,  bool remove = false);
        bool UpdateProcessData(HANDLE pid, PLPROCESS &plprocess);
        bool SendInfoProcessStart(PWSTR pwzname, HANDLE parentPID, HANDLE processId, bool create);
        void EmptyProcessQueue();
        void CopyImagePath(PWSTR pwzname, PPMEVENT ppmevent);
        bool IsParentProcess(PWSTR pwzprocessName, HANDLE hparent, HANDLE processId, DWORD64& parentIndex);
        bool IsChildProcess(PWSTR pwzprocessName, HANDLE hparent, HANDLE processId, DWORD64& parentIndex);
        bool SendProcessStart(PWSTR pwzname, PLPROCESS* pplproc, HANDLE processId, HANDLE threadId);
        void CreateProcessNotifyEx (PEPROCESS perocess, HANDLE processId, PPS_CREATE_NOTIFY_INFO CreateInfo);
        void CreateProcessNotify(HANDLE hParent, HANDLE processId, BOOLEAN create);
        void CreateThreadNotify(HANDLE processId, HANDLE threadId, BOOLEAN create);


        // Static Prototypes
        static void CreateThreadNotifyEntry(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create);
        static void LoadImageNotifyEntry(PUNICODE_STRING pfullImageName, HANDLE processId, PIMAGE_INFO pimageInfo);
        static void CreateProcessNotifyExEntry (PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo);
        static void CreateProcessNotifyEntry(HANDLE hParent, HANDLE ProcessId, BOOLEAN Create);

        // Data
        bool                m_Active;
        DrvQueue            m_ProcessQueue;
        DrvQueue            m_EventQueue;
        FAST_MUTEX          m_hBlackListLock;    // Raises IRQL to APC level
        HANDLE              m_ClientPid;
        HANDLE              m_ClientTid;
        PFILE_OBJECT        m_pKEventObject;
        PPROCESS_LIST       m_pBlackList;

    // Static Data             
        static bool         m_HooksSet;
        static LONG         m_HookUseCount;
    
};










