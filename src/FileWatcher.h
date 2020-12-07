#ifndef __FILEWATCHER_H__
#define __FILEWATCHER_H__

//
// This FileWatcher class in NGPCarMenu plugin is a combination of two different sample implementations.
//
// Source 1: Interface class to use notification callbacks
//              Original author: Sadique Ali 
//              License: No license text and posted in a public blog site, so assuming the license is "use at your own risk for anything for free").
//              Source:  https://cppcodetips.wordpress.com/2019/07/26/file-system-watcher-in-c-for-windows/
//
// Source 2: ASync overlapped use of ReadDirectoryChangeW events
//             Original author: Remy Lebeau
//             License: No license text and posted in a public blog site, so assuming the license is "use at your own risk for anything for free").
//             https://stackoverflow.com/questions/43664998/readdirectorychangesw-and-getoverlappedresult
//
// Combination of these two solutions by MIKA-N. All mistakes and errors because of this merge is not because of those other two samples.
// License: Free to use in binary and source code format. Use at your own risk. No warranty given whatsoever.
//

//
// Class to listen for WinOS file system changes in a specified folder. NGPCarMenuPlugin uses this when RBRRX plugin is used to save replay files in rbr\Replays\ folder.
//

//#include <string>
//#include <windows.h>
//#include <Winbase.h>
//#include <stdlib.h>
//#include <stdio.h>
//#include <tchar.h>
#include <thread>
#include <vector>

#define FILEWATCHER_ERR_NOERROR_CLOSING   0
#define FILEWATCHER_ERR_TERMEVENT       101
#define FILEWATCHER_ERR_DIRHANDLE       102
#define FILEWATCHER_ERR_BUFFERALLOC     103
#define FILEWATCHER_ERR_OVERLAPEVENT    104
#define FILEWATCHER_ERR_OVERLAPPENDING  105
#define FILEWATCHER_ERR_WAITFAILED      106


class CFileSystemWatcher;
extern void fileWatcherThread(CFileSystemWatcher& watcherObj);

class IFileWatcherListener 
{
public:
    virtual void OnFileChange (const std::wstring& path) = 0;
    //virtual void OnFileAdded  (const std::wstring& path) = 0;
    //virtual void OnFileRemoved(const std::wstring& path) = 0;
    //virtual void OnFileRenamed(const std::wstring& path) = 0;
    virtual void OnError      (const int errorCode) = 0;
    
    virtual void DoCompletion(BOOL onlyForceCleanup = FALSE) = 0;
};


//
// CFileWatcher class. Watching a directory passed for file changeand notifies if there is a change
//
class CFileSystemWatcher
{
private:   
    volatile bool m_bRunning;           // Watcher running (TRUE) or stopped (FALSE)

    std::wstring m_sDir;      // File Watcher Directory
    HANDLE m_hDir;            // Handle to watcher directory

    HANDLE m_hTermEvent;      // ASync fileWatcherThread terminator event
    DWORD  m_dwNotifyEvents;  // Type of file events to monitor

    std::vector<IFileWatcherListener*> m_Listeners;     // List of listeners to be notified

    volatile bool m_bThreadRunning;                     // Child thread still running (TRUE) or closed/closing (FALSE)
    std::unique_ptr<std::thread> m_pFileWatcherThread;  // File Watcher Thread

public:
    CFileSystemWatcher(DWORD notifyEvents = FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE)
    {
        m_bRunning = m_bThreadRunning = false;
        m_hTermEvent = nullptr;
        m_hDir = INVALID_HANDLE_VALUE;
        m_dwNotifyEvents = notifyEvents;
    }

    CFileSystemWatcher(const std::wstring& watchDir, DWORD notifyEvents = FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE) : CFileSystemWatcher(notifyEvents)
    { 
        m_sDir = watchDir; 
    }

    ~CFileSystemWatcher() { Stop(TRUE); }

    void  SetDir(const std::wstring& watchDir) { m_sDir = watchDir; }
    const std::wstring& GetDir() { return m_sDir; }
    const HANDLE GetHDir() { return m_hDir; }

    const HANDLE GetTermEvent()       { return m_hTermEvent; }
    const DWORD  GetNotifyEventType() { return m_dwNotifyEvents; }

    void AddFileChangeListener(IFileWatcherListener* listener) { m_Listeners.push_back(listener); }

    void Start()
    {
        if (!m_bRunning)
        {            
            m_hTermEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            if (m_hTermEvent != nullptr)
            {             
                m_hDir = ::CreateFileW(GetDir().c_str(), // pointer to the file name
                    FILE_LIST_DIRECTORY,                // access (read/write) mode
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,  // share mode
                    NULL,                               // security descriptor
                    OPEN_EXISTING,                      // how to create
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, // file attributes
                    NULL                                // file with attributes to copy
                );

                if (m_hDir != INVALID_HANDLE_VALUE)
                {
                    m_bRunning = m_bThreadRunning = true;
                    
                    m_pFileWatcherThread = std::unique_ptr<std::thread>(new std::thread(fileWatcherThread, std::ref(*this)));
                    m_pFileWatcherThread->detach();
                }
                else
                {
                    OnError(FILEWATCHER_ERR_DIRHANDLE);
                    Stop(TRUE);
                }
            }
            else
            {
                OnError(FILEWATCHER_ERR_TERMEVENT);
            }
        }
    }

    void Stop(BOOL forceCleanup = FALSE) 
    {
        if (m_bRunning || forceCleanup)
        {
            if (m_hTermEvent != nullptr)
            {
                HANDLE hTmpEventHandle = m_hTermEvent;
                m_hTermEvent = nullptr;                

                SetEvent(hTmpEventHandle);

                // Ignore VC++ compitler warning about GetTickCount wrapping around if the PC is running more tan 49 days without shutting down. 
                // This routine doesn't care about it because the tick diff still works even when counter wrapped (DWORD is UNSIGNED data type, not signed value so the diff would wrap also)
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:28159)
#endif
                DWORD dwStartTick = GetTickCount();
                while (m_bThreadRunning && (GetTickCount() - dwStartTick) > 5000)
                    Sleep(100);
                CloseHandle(hTmpEventHandle);
#if defined(_MSC_VER) 
#pragma warning(pop)
#endif
            }

            if (m_hDir != INVALID_HANDLE_VALUE)
            {
                CloseHandle(m_hDir);
                m_hDir = INVALID_HANDLE_VALUE;
            }

            m_bRunning = m_bThreadRunning = false;
        }
    }
    
    volatile bool Running() const   { return m_bRunning; }
    void Running(volatile bool val) { m_bRunning = val; }

    volatile bool ThreadRunning() const   { return m_bThreadRunning; }
    void ThreadRunning(volatile bool val) { m_bThreadRunning = val; }

    /*!
     * Funtion called when a file is chnaged in watcher directory
     *
     * \param fileNmae
     */
    void OnFileChange(const std::wstring& sFile) 
    {
        for (auto& listener : m_Listeners)
            listener->OnFileChange(sFile);
    }


    /*!
     *
     * Funtion called when a file is added to watch directory
     *
     * \param sFile
     */
    //void OnFileAdded(const std::wstring& sFile)
    //{
    //    for (auto& listener : m_Listeners)
    //        listener->OnFileAdded(sFile);
    //}

    /*!
     * Funtion called when a file is renamed from watch directory
     *
     * \param sFile
     */
    //void OnFileRenamed(const std::wstring& sFile)
    //{
    //   for (auto& listener : m_Listeners)
    //        listener->OnFileRenamed(sFile);
    //}

    /*!
     * Funtion called when a file is removed from watch directory
     *
     * \param sFile
     */
    //void OnFileRemoved(const std::wstring& sFile)
    //{
    //    for (auto& listener : m_Listeners)
    //        listener->OnFileRemoved(sFile);
    //}

    /*!
     * Funtion called when there was an error while waiting file changes (except errorCode==0 is not an error case, but a thread closing notification)
     *
     * \param sFile
     */
    void OnError(const int errorCode)
    {
        for (auto& listener : m_Listeners)
            listener->OnError(errorCode);
    }
};

#endif
