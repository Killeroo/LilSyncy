#pragma once

#include <Windows.h>
#include <vector>
#include <map>
#include <string>

#include "SafeQueue.h"

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

enum Instruction : byte
{
    COPY,
    REPLACE,
    REMOVE,
};

enum Colors : WORD
{
    GREEN = 2,
    RED = 4,
    MAGENTA = 5,
    YELLOW = 6,
    WHITE = 7
};

class LilSyncy
{
public:
    int Run(int argc, wchar_t* argv[]);

private:
    void ParseArguments(int argc, wchar_t* argv[]);
    void CalculateFolderDifferences(std::map<std::wstring, FileData>& sourceFiles, std::map<std::wstring, FileData>& destinationFiles);
    void PerformSync();
    void Cleanup();

    std::wstring GetLastErrorAsString();

private:
    SyncOptions Options;
    std::queue<std::tuple<Instruction, std::wstring>> SyncInstructions;
    std::vector<std::wstring> FoldersToCreate;
    std::vector<std::wstring> FoldersToDelete;
    unsigned long ItemsCopied = 0;
    unsigned long ItemsDeleted = 0;
    unsigned long Errors = 0;

public:
    static void LogMessage(Colors color, const wchar_t* format, ...);
    static bool DoesDirectoryExist(std::wstring& pathToDirectory);
};