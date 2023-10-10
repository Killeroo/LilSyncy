#include "FileWalker.h"
#include <tchar.h>

std::map<std::wstring, FileData> FileWalker::GetFiles(std::wstring path)
{
    //std::wstring path = TEXT("C:\\Projects\\");// TEXT("C:\\");
    Reset();

    RootPath = path;
    std::thread threads[FILE_WALKER_THREAD_COUNT];

    // Add the initial path
    PathsToProcess.enqueue(path);

    // Start worker threads
    for (int i = 0; i < FILE_WALKER_THREAD_COUNT; i++)
    {
        threads[i] = std::thread(&FileWalker::WorkerThread, this, std::ref(ThreadFinishedFlags[i]));

        Sleep(10);
    }

    // Wait for the threads to finish
    char loadingCharacter[4] = { '|', '/', '-', '\\' };
    uint8_t currentLoadingCharacter = 0;
    std::wstring displayPath = RootPath;
    if (RootPath.size() > 30)
    {
        displayPath.erase(30, RootPath.size() - 30);
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
        _tprintf(TEXT("Discovered %d files in '%s' [%c] \r"), FoundFiles.size(), displayPath.c_str(), loadingCharacter[currentLoadingCharacter]);

        Sleep(50);
    }
    _tprintf(TEXT("Discovered %d files in '%s' [X] \n"), FoundFiles.size(), displayPath.c_str());

    // Wait for them all the join so they can be disposed properly 
    for (int i = 0; i < FILE_WALKER_THREAD_COUNT; i++)
    {
        threads[i].join();
    }

    // Prepare results
    std::map<std::wstring, FileData> Results;
    const size_t rootPathSize = RootPath.size();
    FileData currentFile;
    while (FoundFiles.empty() == false)
    {
        currentFile = FoundFiles.dequeue();

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
    while (PathsToProcess.empty() == false)
    {
        currentPath = PathsToProcess.dequeue();

        // TODO: There is a chance that threads are choking on too few things to process to start with
        // turn on this logging and see them each adding multiple folders at once.
        // Add some logging to see if they are going through the same files and make them focus on different things...
        //_tprintf(TEXT("%s\n"), currentPath.c_str());

        hFind = FindFirstFile((currentPath + L"*").c_str(), &FoundFileData);
        do
        {
            if (FoundFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                currentFilename = FoundFileData.cFileName;

                if (currentFilename != L"." && currentFilename != L"..")
                {
                    // TODO: Cleanup - D.R.Y
                    FileData data;
                    data.Name = FoundFileData.cFileName;
                    data.Path = currentPath;
                    data.LastWriteTime = FoundFileData.ftLastWriteTime;
                    data.Size = (FoundFileData.nFileSizeHigh * MAXDWORD) + FoundFileData.nFileSizeLow;
                    data.Directory = true;
                    FoundFiles.enqueue(data);


                    //_tprintf(TEXT("-> %s %s\n"), currentPath.c_str(), currentFilename.c_str());

                    PathsToProcess.enqueue(currentPath + FoundFileData.cFileName + L"\\");
                }
            }
            else// if (FoundFileData.dwFileAttributes & FILE_ATTRIBUTE_NORMAL)
            {
                FileData data;
                data.Name = FoundFileData.cFileName;
                data.Path = currentPath;
                data.LastWriteTime = FoundFileData.ftLastWriteTime;
                data.Size = (FoundFileData.nFileSizeHigh * MAXDWORD) + FoundFileData.nFileSizeLow;
                data.Directory = false;
                FoundFiles.enqueue(data);
                

                //_tprintf(TEXT("*> %s\n"), currentPath.c_str());
            }
        } while (FindNextFile(hFind, &FoundFileData));

        FindClose(hFind);
    }

    // Signal that we are done
    IsFinished = true;
}

void FileWalker::Reset()
{
    FoundFiles.clear();
    PathsToProcess.clear();
    for (int i = 0; i < FILE_WALKER_THREAD_COUNT; i++)
    {
        ThreadFinishedFlags[i] = false;
    }
    RootPath.clear();
}