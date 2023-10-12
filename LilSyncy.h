#pragma once

#include <Windows.h>
#include <string>

struct FileData
{
    std::wstring Name;
    std::wstring Path; // TODO: Remove this
    int64_t Size;
    FILETIME LastWriteTime;

    bool Directory;
};

struct SyncOptions
{
    std::wstring SourcePath;
    std::wstring DestinationPath;

    bool DryRun;
};

enum Instruction : byte
{
    COPY,
    REPLACE,
    REMOVE,
};

enum Colors : short
{
    GREEN = 2,
    RED = 4,
    YELLOW = 6,
    WHITE = 7
};

class LilSyncy
{

public:
    // TODO: Rename
    int Run(int argc, wchar_t* argv[]);

private:
    void ParseArguments(int argc, wchar_t* argv[]);
    void CalculateFolderDifferences(std::map<std::wstring, FileData>& sourceFiles, std::map<std::wstring, FileData>& destinationFiles);
    void PerformSync();
    void PrintHelp();

private:
    SyncOptions Options;
    // TODO: Remove SafeQueue
    SafeQueue<std::tuple<Instruction, std::wstring>> SyncInstructions;
    std::vector<std::wstring>& FoldersToCreate;
    std::vector<std::wstring>& FoldersToDelete;

public:
    static void LogMessage(Colors color, const wchar_t* format, ...);
    static bool DoesDirectoryExist(std::wstring& pathToDirectory);

private:
    static HANDLE ConsoleHandle;
    static CONSOLE_SCREEN_BUFFER_INFO ConsoleScreenBufferInfo;
    static WORD CurrentConsoleColor;
};