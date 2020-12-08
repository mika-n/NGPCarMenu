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

#include "stdafx.h"

//#include <iostream>
#include <thread>
//#include <future> 
#include <vector>

#include "FileWatcher.h"

void fileWatcherThread(CFileSystemWatcher& watcherObj)
{
    //std::vector<BYTE> buffer(1024 * 64);
    BYTE* buffer = nullptr; 
    OVERLAPPED o { 0 };
    DWORD dwBytesReturned = 0;

    bool  bPending = false;
    bool  bKeepRunning = true;

    std::wstring sFileName;

    o.hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
    if (o.hEvent == nullptr)
    {
        watcherObj.OnError(FILEWATCHER_ERR_OVERLAPEVENT);
        return;
    }

    HANDLE hWaitEvents[2] = { o.hEvent, watcherObj.GetTermEvent() };
    HANDLE hDir = watcherObj.GetHDir();

    try
    {
        buffer = (BYTE*) _aligned_malloc(1024 * 64, sizeof(DWORD)); /// ReadDirectoryChangesW requires that the buffer is DWORD aligned

        do
        {
            if (buffer == nullptr)
            {
                watcherObj.OnError(FILEWATCHER_ERR_BUFFERALLOC);
                break;
            }

            if (!watcherObj.Running())
                break;

            // Wait file events in hDir/sDir without blocking (overlapped async)
            bPending = ::ReadDirectoryChangesW(hDir, /*&buffer[0], buffer.size(),*/ buffer, 1024 * 64, FALSE, watcherObj.GetNotifyEventType(), &dwBytesReturned, &o, NULL);

            if (!bPending)
            {
                watcherObj.OnError(FILEWATCHER_ERR_OVERLAPPENDING);
                break;
            }

            switch (::WaitForMultipleObjects(2, hWaitEvents, FALSE, INFINITE))
            {
            case WAIT_OBJECT_0:
            {
                if (!::GetOverlappedResult(hDir, &o, &dwBytesReturned, TRUE))
                    break;

                bPending = false;
                if (dwBytesReturned == 0)
                    break;

                FILE_NOTIFY_INFORMATION* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(/*&buffer[0]*/ buffer);
                do
                {
                    if (fni->Action != 0)
                        sFileName.assign(fni->FileName, fni->FileNameLength / sizeof(WCHAR));

                    switch (fni->Action)
                    {
                        case FILE_ACTION_MODIFIED:
                        case FILE_ACTION_ADDED:
                        case FILE_ACTION_RENAMED_NEW_NAME:
                            watcherObj.OnFileChange(sFileName);
                            break;

                        //case FILE_ACTION_REMOVED:
                        //    watcherObj.OnFileRemoved(sFileName);
                        //    break;
                    }

                    if (fni->NextEntryOffset == 0)
                        break;

                    fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<BYTE*>(fni) + fni->NextEntryOffset);
                } while (true);
                break;
            }

            case WAIT_OBJECT_0 + 1:
                bKeepRunning = false; // Thread signaled to quit
                break;

            case WAIT_FAILED:
                watcherObj.OnError(FILEWATCHER_ERR_WAITFAILED);
                break;
            }
        } while (bKeepRunning && watcherObj.Running());

        if (bPending)
        {
            ::CancelIo(hDir);
            ::GetOverlappedResult(hDir, &o, &dwBytesReturned, TRUE);
        }
    }
    catch (...)
    {
        // Eat all exceptions
    }

    if(o.hEvent != nullptr) ::CloseHandle(o.hEvent);
    if(buffer != nullptr) ::_aligned_free(buffer);

    watcherObj.ThreadRunning(FALSE); // Signal watcherObj that this thread is now closing

    // Not an error, just notifying the caller that watcher thread is now closing (caller can just ignore this errorCode if there is no need to do any special things when this thread handler is closed)
    watcherObj.OnError(FILEWATCHER_ERR_NOERROR_CLOSING);
}

