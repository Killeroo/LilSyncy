#pragma once

#include <Windows.h>
#include <vector>
#include <map>
#include <string>

#include "SafeQueue.h"

// TODO: Comments

enum SyncInstruction : byte
{
    COPY,
    REPLACE,
    REMOVE,
};

enum ConsoleColors : WORD
{
    GREEN = 2,
    RED = 4,
    MAGENTA = 5,
    YELLOW = 6,
    WHITE = 7
};

struct FileData
{
    // TODO: With the relative path used as a key we probably don't need this so we could remove it at some point as it can take up a lot of memory for bigger scans
    std::wstring Path;

    std::wstring Name;
    int64_t Size = 0;
    FILETIME LastWriteTime;
    bool Directory = false;
};

struct SyncOptions
{
    std::wstring SourcePath;
    std::wstring DestinationPath;
    bool DryRun = false;
};

// TODO: Include size
struct FileOperation
{
    std::wstring Filename;
    SyncInstruction Operation;
};

class LilSyncy // TODO: Rename
{
public:
    int Run(int argc, wchar_t* argv[]);

    const FileOperation GetCurrentOperation() { return CurrentOperation; }
    const size_t GetTotalInstructions() { return OperationCount; }
    const size_t GetProcessedInstructions() { return OperationsPerformed; }

private:
    void ParseArguments(int argc, wchar_t* argv[]);
    void CalculateFolderDifferences(std::map<std::wstring, FileData>& sourceFiles, std::map<std::wstring, FileData>& destinationFiles);
    void PerformSync();
    void Cleanup();

private:
    /* Copy file progress callback for CopyFileEx */
    static DWORD CALLBACK CopyFileProgress(
        LARGE_INTEGER TotalFileBytes,
        LARGE_INTEGER TotalBytesTransferred,
        LARGE_INTEGER StreamSize,
        LARGE_INTEGER StreamSizeTransferred,
        DWORD dwStreamNumber,
        DWORD dwCallbackReason,
        HANDLE hSourceFile,
        HANDLE hDestinationFile,
        LPVOID lpData);

private:
    std::wstring GetLastErrorAsString();

private:
    SyncOptions Options;
    std::queue<std::tuple<SyncInstruction, std::wstring>> SyncInstructions; // TODO: Could change to FileOperation
    std::vector<std::wstring> FoldersToCreate;
    std::vector<std::wstring> FoldersToDelete;
    unsigned long ItemsCopied = 0;
    unsigned long ItemsDeleted = 0;
    unsigned long Errors = 0;

    size_t OperationCount = 0;
    size_t OperationsPerformed = 0;

    FileOperation CurrentOperation;

public:
    static void LogMessage(ConsoleColors color, const wchar_t* format, ...);
    static bool DoesDirectoryExist(std::wstring& pathToDirectory);
};