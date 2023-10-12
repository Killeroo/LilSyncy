#pragma once

#include "FileData.h"
#include "LilSyncy.h"
#include "SafeQueue.h"
#include "Constants.h"

#include <map>
#include <mutex>

class FileWalker
{
public:
    std::map<std::wstring, FileData> GetFiles(std::wstring path);

private:
    void WorkerThread(std::atomic<bool>& IsFinished);
    void Reset();

private:
    SafeQueue<FileData> FoundFiles;
    SafeQueue<std::wstring> PathsToProcess;
    std::atomic<bool> ThreadFinishedFlags[FILE_WALKER_THREAD_COUNT];
    std::wstring RootPath;
};