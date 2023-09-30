// LilSyncy.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>
#include <tchar.h>
#include <queue>

#include <mutex>
#include <condition_variable>

void PrintDirectory(std::wstring _path);

using string = std::wstring;

//////////////////////////////////////////////////
// A threadsafe-queue.
// Copied from: https://stackoverflow.com/a/16075550
//////////////////////////////////////////////////
template <class T>
class SafeQueue
{
public:
    SafeQueue(void)
        : q()
        , m()
        , c()
    {}

    ~SafeQueue(void)
    {}

    // Add an element to the queue.
    void enqueue(T t)
    {
        std::lock_guard<std::mutex> lock(m);
        q.push(t);
        c.notify_one();
    }

    // Get the "front"-element.
    // If the queue is empty, wait till a element is avaiable.
    T dequeue(void)
    {
        std::unique_lock<std::mutex> lock(m);
        while (q.empty())
        {
            // release lock as long as the wait and reaquire it afterwards.
            c.wait(lock);
        }
        T val = q.front();
        q.pop();
        return val;
    }

    bool empty()
    {
        // Is this needed???????, I don't think we need this lock 
        std::lock_guard<std::mutex> lock(m);
        return q.empty();
    }

    std::queue<T> getQueue()
    {
        return q;
    }

private:
    std::queue<T> q;
    mutable std::mutex m;
    std::condition_variable c;
};
//////////////////////////////////////////////////

struct FileData
{
    string Name;
    string Path;
    int64_t Size;
    FILETIME LastWriteTime;
};


//SafeQueue<string> ThreadsafePathQueue;
//std::atomic<long> TotalProcessedPaths = 0;
void WalkPath(std::vector<FileData>& FoundFiles, SafeQueue<string>& PathsToProcess, std::atomic<long>& TotalProcessedPaths, std::atomic<bool>& IsFinished)
{
    //_tprintf(TEXT("-> Started\n"));

    WIN32_FIND_DATA FoundFileData;
    HANDLE hFind;
    string currentPath, currentFilename;
    while (PathsToProcess.empty() == false)
    {
        currentPath = PathsToProcess.dequeue();

        //_tprintf(TEXT("%s\n"), currentPath.c_str());

        hFind = FindFirstFile((currentPath + L"*").c_str(), &FoundFileData);
        do
        {
            TotalProcessedPaths++;

            if (FoundFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                currentFilename = FoundFileData.cFileName;
                
                if (currentFilename != L"." && currentFilename != L"..")
                {
                    //_tprintf(TEXT("---- %s\n"), FoundFileData.cFileName);

                    PathsToProcess.enqueue(currentPath + L"\\" + FoundFileData.cFileName + L"\\");
                }
                //else
                //{
                //    //_tprintf(TEXT("[ ] %s\n"), FoundFileData.cFileName);
                //}
            }
            else// if (FoundFileData.dwFileAttributes & FILE_ATTRIBUTE_NORMAL)
            {
                FileData data;
                data.Name = FoundFileData.cFileName;
                data.Path = currentPath;
                data.LastWriteTime = FoundFileData.ftLastWriteTime;
                data.Size = (FoundFileData.nFileSizeHigh * MAXDWORD) + FoundFileData.nFileSizeLow;
                FoundFiles.push_back(data);

                //_tprintf(TEXT("-> %s\n"), FoundFileData.cFileName);
                //_tprintf(TEXT("-> %s\n"), currentPath.c_str());
            }
        } while (FindNextFile(hFind, &FoundFileData));

        FindClose(hFind);
    }

    // Signal that we are done
    IsFinished = true;
}

//namespace StaticTest
//{
//    SafeQueue<string> ThreadsafePathQueue;
//    std::atomic<long> TotalProcessedPaths = 0;
//    void WalkPath()
//    {
//        _tprintf(TEXT("-> Started\n"));
//
//        WIN32_FIND_DATA FoundFileData;
//        HANDLE hFind;
//        string currentPath, currentFilename;
//        while (ThreadsafePathQueue.empty() == false)
//        {
//            currentPath = ThreadsafePathQueue.dequeue();
//
//            //_tprintf(TEXT("%s\n"), currentPath.c_str());
//
//            hFind = FindFirstFile((currentPath + L"*").c_str(), &FoundFileData);
//            do
//            {
//                TotalProcessedPaths++;
//
//                if (FoundFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
//                {
//                    currentFilename = FoundFileData.cFileName;
//
//                    if (currentFilename == L"." || currentFilename == L"..")
//                    {
//                        //_tprintf(TEXT("---- %s\n"), FoundFileData.cFileName);
//                    }
//                    else
//                    {
//                        //_tprintf(TEXT("[ ] %s\n"), FoundFileData.cFileName);
//                        ThreadsafePathQueue.enqueue(currentPath + L"\\" + FoundFileData.cFileName + L"\\");
//                    }
//                }
//                else
//                {
//                    //_tprintf(TEXT("-> %s\n"), FoundFileData.cFileName);
//                }
//            } while (FindNextFile(hFind, &FoundFileData));
//
//            FindClose(hFind);
//        }
//    }
//}

class FileWalker
{
public:
    std::vector<FileData> GetFiles(string path)
    {

    }

private:


};

int main()
{
    //const char* path = "C:\\*";
    //LPCWSTR path = L"C:\\*";
    std::wstring path = TEXT("C:\\Projects\\");// TEXT("C:\\");


    SafeQueue<string> paths;
    std::atomic<long> pathCount;

    // Add the initial path
    paths.enqueue(path);
    //StaticTest::ThreadsafePathQueue.enqueue(path);

    constexpr int THREAD_COUNT = 10;
    std::vector<FileData> results[THREAD_COUNT];
    std::thread threads[THREAD_COUNT];
    std::atomic<bool> completionFlags[THREAD_COUNT];
    //std::vector<std::thread> threads;
    for (int i = 0; i < THREAD_COUNT; i++)
    {
        completionFlags[i] = false;
        threads[i] = std::thread(WalkPath, std::ref(results[i]), std::ref(paths), std::ref(pathCount), std::ref(completionFlags[i]));
        //threads.push_back(std::thread(WalkPath, std::ref(paths), std::ref(pathCount), std::ref(completionFlags[i])));

        Sleep(10);

        //threads[i] = std::thread(StaticTest::WalkPath);
    }

    char loadingCharacter[4] = { '|', '/', '-', '\\' };

    // Wait for the threads to finish
    bool finished = false;
    int16_t current = 0;
    while (!finished)
    {
        // Check if the threads have finished running
        finished = true;
        for (int i = 0; i < THREAD_COUNT; i++)
        {
            finished &= completionFlags[i];
        }

        current++;
        if (current == 4)
        {
            current = 0;
        }

        printf("Finding files %c \r", loadingCharacter[current]);

        Sleep(50);
    }

    // Wait for them all the join so they can be disposed properly 
    for (int i = 0; i < THREAD_COUNT; i++)
    {
        threads[i].join();
    }

    for (int c = 0; c < THREAD_COUNT; c++)
    {
        std::vector<FileData> current = results[c];
        for (int i = 0; i < current.size(); i++)
        {
            //results->data();
            //printf("%s\n", current[i].c_str());

            _tprintf(TEXT("-> %s \n"), current[i].Name.c_str());
        }
    }



    //std::thread first(WalkPath);
    //Sleep(10);
    //std::thread second(WalkPath);
    //std::thread third(WalkPath);
    //std::thread fourth(WalkPath);

    //first.join();                // pauses until first finishes
    //second.join();               // pauses until second finishes
    //third.join();               // pauses until second finishes
    //fourth.join();               // pauses until second finishes

    std::cout << "completed. \n" << pathCount;

    return 0;


    //std::queue<string> pathQueue;
    //
    //// Add the initial path
    //pathQueue.push(path);

    //WIN32_FIND_DATA FoundFileData;
    //HANDLE hFind;
    //string currentPath, currentFilename;
    //while (pathQueue.size() > 0)
    //{
    //    currentPath = pathQueue.front();

    //    //_tprintf(TEXT("%s\n"), currentPath.c_str());

    //    hFind = FindFirstFile((currentPath + L"*").c_str(), &FoundFileData);
    //    do
    //    {
    //        TotalProcessedPaths++;
    //        if (FoundFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    //        {
    //            currentFilename = FoundFileData.cFileName;

    //            if (currentFilename == L"." || currentFilename == L"..")
    //            {
    //                //_tprintf(TEXT("---- %s\n"), FoundFileData.cFileName);
    //            }
    //            else
    //            {
    //                //_tprintf(TEXT("[ ] %s\n"), FoundFileData.cFileName);
    //                pathQueue.push(currentPath + L"\\" + FoundFileData.cFileName + L"\\");
    //            }
    //        }
    //        else
    //        {
    //            //_tprintf(TEXT("-> %s\n"), FoundFileData.cFileName);
    //        }
    //    } while (FindNextFile(hFind, &FoundFileData));

    //    FindClose(hFind);

    //    // Remove processed path from the queue
    //    pathQueue.pop();
    //}


    //std::cout << "completed. \n" << TotalProcessedPaths;





    //WIN32_FIND_DATA FoundFileData;
    //HANDLE hFind;


    //hFind = FindFirstFile((path + L"*").c_str(), &FoundFileData);
    //do
    //{
    //    if (FoundFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    //    {
    //        _tprintf(TEXT("[ ] %s\n"), FoundFileData.cFileName);
    //        PrintDirectory(path + L"\\" + FoundFileData.cFileName + L"\\*");

    //    }
    //    else
    //    {
    //        _tprintf(TEXT("-> %s\n"), FoundFileData.cFileName);
    //    }
    //}
    //while (FindNextFile(hFind, &FoundFileData));

    //FindClose(hFind);





    //FindClose(hFind);

    //WIN32_FIND_DATA FindFileData;
    //HANDLE hFind;


    //hFind = FindFirstFile("C:\\path\\*", &FindFileData);
    //if (hFind == INVALID_HANDLE_VALUE)
    //{
    //    printf("FindFirstFile failed (%d)\n", GetLastError());
    //    return;
    //}
    //else
    //{
    //    _tprintf(TEXT("The first file found is %s\n"),
    //        FindFileData.cFileName);
    //    FindClose(hFind);
    //}
}



void PrintDirectory(std::wstring _path)
{
    WIN32_FIND_DATA FoundFileData;
    HANDLE hFind = FindFirstFile(_path.c_str(), &FoundFileData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        return;
    }

    do
    {
        if (FoundFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            _tprintf(TEXT("   [ ] %s\n"), FoundFileData.cFileName);
        }
        else
        {
            _tprintf(TEXT("   -> %s\n"), FoundFileData.cFileName);
        }
    } while (FindNextFile(hFind, &FoundFileData));

    FindClose(hFind);
}