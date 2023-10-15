#include "FileWalker.h"
#include "LilSyncy.h"
#include <tchar.h>

std::map<std::wstring, FileData> FileWalker::GetFiles(std::wstring path)
{
    Reset();

    RootPath = path;
    std::thread threads[FILE_WALKER_THREAD_COUNT];

    // Add the initial path
    PathsToProcess.Enqueue(path);

    // Start worker threads
    for (int i = 0; i < FILE_WALKER_THREAD_COUNT; i++)
    {
        threads[i] = std::thread(&FileWalker::WorkerThread, this, std::ref(ThreadFinishedFlags[i]));

        Sleep(10);
    }

    // Wait for the threads to finish
    constexpr char loadingCharacter[4] = { '|', '/', '-', '\\' };
    uint8_t currentLoadingCharacter = 0;
    std::wstring displayPath = RootPath;
    if (RootPath.size() > 60)
    {
        displayPath.erase(60, RootPath.size() - 60);
        displayPath.append(L"...");
    }
    bool finished = false;
    while (!finished)
    {
        // Check if the threads have finished running
        finished = true;
        for (int i = 0; i < FILE_WALKER_THREAD_COUNT; i++)
        {
            finished &= ThreadFinishedFlags[i];
        }

        // Display a cute little loading character
        currentLoadingCharacter++;
        if (currentLoadingCharacter == 4)
        {
            currentLoadingCharacter = 0;
        }
        _tprintf(TEXT("Discovered %zu files in '%s' [%c] \r"), FoundFiles.Size(), displayPath.c_str(), loadingCharacter[currentLoadingCharacter]);

        Sleep(50);
    }
    _tprintf(TEXT("Discovered %zu files in '%s' [X] \n"), FoundFiles.Size(), displayPath.c_str());

    // Wait for them all the join so they can be disposed properly 
    for (int i = 0; i < FILE_WALKER_THREAD_COUNT; i++)
    {
        threads[i].join();
    }

    // Prepare results
    std::map<std::wstring, FileData> Results;
    const size_t rootPathSize = RootPath.size();
    FileData currentFile;
    while (FoundFiles.Empty() == false)
    {
        currentFile = FoundFiles.Dequeue();

        std::wstring relativePath = currentFile.Path + currentFile.Name;
        relativePath.erase(0, rootPathSize);

        Results.insert({relativePath, currentFile});
    }

    return Results;
}

void FileWalker::WorkerThread(std::atomic<bool>& IsFinished)
{
    WIN32_FIND_DATA FoundFileData;
    HANDLE hFind;
    std::wstring currentPath, currentFilename;
    
    // The idea here is that all threads have access to the PathsToProcess queue and will add new folders to it as they
    // walk through the directory tree. For each path to process we list all files and folders, adding both to the foundfiles queue
    // (which will be processed later), each time we find a directory we add that to PathsToProcess to be processed by this or another th
    while (PathsToProcess.Empty() == false)
    {
        currentPath = PathsToProcess.Dequeue();

        // TODO: There is a chance that threads are choking on too few things to process to start with
        // turn on this logging and see them each adding multiple folders at once.
        // Add some logging to see if they are going through the same files and make them focus on different things...
        //_tprintf(TEXT("%s\n"), currentPath.c_str());

        hFind = FindFirstFile((currentPath + L"*").c_str(), &FoundFileData);
        do
        {
            currentFilename = FoundFileData.cFileName;
            if (currentFilename == L"." || currentFilename == L"..")
            {
                continue;
            }

            // Save file data for later 
            // (at this point it could be either a file or a directory)
            FileData data;
            data.Name = FoundFileData.cFileName;
            data.Path = currentPath;
            data.LastWriteTime = FoundFileData.ftLastWriteTime;
            data.Size = (FoundFileData.nFileSizeHigh * MAXDWORD) + FoundFileData.nFileSizeLow;

            if (FoundFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                data.Directory = true;

                // Process this directory
                PathsToProcess.Enqueue(currentPath + FoundFileData.cFileName + L"\\");
            }

            // Add entry data to queue
            FoundFiles.Enqueue(data);

        } while (FindNextFile(hFind, &FoundFileData));

        FindClose(hFind);
    }

    //while (PathsToProcess.Empty() == false)
    //{
    //    currentPath = PathsToProcess.Dequeue();

    //    // TODO: There is a chance that threads are choking on too few things to process to start with
    //    // turn on this logging and see them each adding multiple folders at once.
    //    // Add some logging to see if they are going through the same files and make them focus on different things...
    //    //_tprintf(TEXT("%s\n"), currentPath.c_str());

    //    hFind = FindFirstFile((currentPath + L"*").c_str(), &FoundFileData);
    //    do
    //    {
    //        if (FoundFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    //        {
    //            currentFilename = FoundFileData.cFileName;

    //            if (currentFilename != L"." && currentFilename != L"..")
    //            {
    //                // TODO: Cleanup - D.R.Y
    //                FileData data;
    //                data.Name = FoundFileData.cFileName;
    //                data.Path = currentPath;
    //                data.LastWriteTime = FoundFileData.ftLastWriteTime;
    //                data.Size = (FoundFileData.nFileSizeHigh * MAXDWORD) + FoundFileData.nFileSizeLow;
    //                data.Directory = true;
    //                FoundFiles.Enqueue(data);

    //                PathsToProcess.Enqueue(currentPath + FoundFileData.cFileName + L"\\");
    //            }
    //        }
    //        else // 
    //        {
    //            // TODO: We could be filtering by checking file attributed here (eg. `else if (FoundFileData.dwFileAttributes & FILE_ATTRIBUTE_NORMAL)`)
    //            FileData data;
    //            data.Name = FoundFileData.cFileName;
    //            data.Path = currentPath;
    //            data.LastWriteTime = FoundFileData.ftLastWriteTime;
    //            data.Size = (FoundFileData.nFileSizeHigh * MAXDWORD) + FoundFileData.nFileSizeLow;
    //            data.Directory = false;
    //            FoundFiles.Enqueue(data);
    //        }
    //    } while (FindNextFile(hFind, &FoundFileData));

    //    FindClose(hFind);
    //}

    // Signal that this thread is finished
    IsFinished = true;
}

void FileWalker::Reset()
{
    FoundFiles.Clear();
    PathsToProcess.Clear();
    for (int i = 0; i < FILE_WALKER_THREAD_COUNT; i++)
    {
        ThreadFinishedFlags[i] = false;
    }
    RootPath.clear();
}