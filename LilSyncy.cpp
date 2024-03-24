// LilSyncy.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>
#include <tchar.h>
#include <queue>
#include <locale>
#include <codecvt>
#include <string>
#include <iostream>
#include <map>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <chrono>

#include "FileWalker.h"
#include "LilSyncy.h"

// Would love for this to live in the header and be closed whenever the program abruptly ends but this works for now..
static HANDLE ConsoleHandle = NULL;

int LilSyncy::Run(int argc, wchar_t* argv[])
{
    ParseArguments(argc, argv);

    if ((Options.DestinationPath == L"" || Options.SourcePath == L"")
        || Options.DestinationPath == Options.SourcePath)
    {
        LilSyncy::LogMessage(RED, TEXT("Could not find valid source and destination folders. Please check and try again."));
        return 1;
    }

    const std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();

    // Gather files
    FileWalker walker;
    std::map<std::wstring, FileData> sourceFiles = walker.GetFiles(Options.SourcePath);
    std::map<std::wstring, FileData> destinationFiles = walker.GetFiles(Options.DestinationPath);

    // Work out differences and sync the files
    CalculateFolderDifferences(sourceFiles, destinationFiles);
    PerformSync();

    // Prints summary
    const std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now();
    const long long ellapsedMilliseconds = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
    _tprintf(TEXT("Sync complete in %lld seconds. %lu items synced (%lu copied, %lu removed) with %lu errors."),
        ellapsedMilliseconds,
        ItemsCopied + ItemsDeleted,
        ItemsCopied,
        ItemsDeleted,
        Errors);

    Cleanup();
    return 0;
}


void LilSyncy::ParseArguments(int argc, wchar_t* argv[])
{
    if (argc == 1)
    {
        printf("LilSyncy (v%.1f) \n%s", VERSION, USAGE_TEXT);
        std::exit(0);
    }

    for (int i = 0; i < argc; i++)
    {
        const std::wstring argument = argv[i];
        std::wstring nextArgument = i + 1 < argc ? argv[i + 1] : L"";

        if (argument == L"--source")
        {
            // Check there is something following the argument 
            // and check that it's a valid path
            if (nextArgument != L"")
            {
                if (DoesDirectoryExist(nextArgument))
                {
                    // Sometimes we can get valid path without the closing slash
                    // (winapi won't like this so lets add it in ourselves)
                    if (nextArgument.back() != '\\')
                    {
                        nextArgument.push_back('\\');
                    }

                    Options.SourcePath = nextArgument;
                }
            }
        }
        else if (argument == L"--destination")
        {
            if (nextArgument != L"")
            {
                if (DoesDirectoryExist(nextArgument))
                {
                    if (nextArgument.back() != '\\')
                    {
                        nextArgument.push_back('\\');
                    }

                    Options.DestinationPath = nextArgument;
                }
            }
        }
        else if (argument == L"--dryrun")
        {
            Options.DryRun = true;
        }
        else if (argument == L"--help" || argument == L"-help" || argument == L"/help" || argument == L"/?")
        {
            printf("LilSyncy (v%.1f) \n%s", VERSION, USAGE_TEXT);
            std::exit(0);
        }
    }
}

void LilSyncy::CalculateFolderDifferences(std::map<std::wstring, FileData>& sourceFiles, std::map<std::wstring, FileData>& destinationFiles)
{
    // Ok lets work out what files we need to sync
    // We loop through the sources files as they are our 'source of truth'
    for (auto const& entry : sourceFiles)
    {
        // First check if the file is missing in the destination.
        // Copy it if so.
        if (destinationFiles.count(entry.first) == 0)
        {
            if (entry.second.Directory == false)
            {
                SyncInstructions.push(std::make_tuple(COPY, entry.first));
            }
            else
            {
                FoldersToCreate.push_back(Options.DestinationPath + entry.first);
            }

            continue;
        }

        // Next check to see if the sizes match
        if (destinationFiles[entry.first].Size != entry.second.Size)
        {
            if (entry.second.Directory == false)
            {
                SyncInstructions.push(std::make_tuple(REPLACE, entry.first));
                continue;
            }
        }

        // TODO: Check last write time..

        // TODO: Check file signature..
    }

    // Save files and folders that we need to remove from the destination
    // We store files in a seperate list because we want to remove them after we have deleted the files that
    // were in them
    for (auto const& entry : destinationFiles)
    {
        // Remove any files in the destination that don't exist in the source
        if (sourceFiles.count(entry.first) == 0)
        {
            if (entry.second.Directory == false)
            {
                SyncInstructions.push(std::make_tuple(REMOVE, entry.first));
            }
            else
            {
                std::wstring currentDir = entry.second.Path + entry.second.Name;
                FoldersToDelete.push_back(currentDir);
            }
        }
    }

    // Sort the folders lists. We want to delete folders largest to smallest and create folders smallest to largest (based on path length),
    // this is because windows complains if you try and add or delete a folder before it's been created/deleted
    std::sort(FoldersToDelete.begin(), FoldersToDelete.end(), [](std::wstring left, std::wstring right) {return left.size() > right.size(); });
    std::sort(FoldersToCreate.begin(), FoldersToCreate.end(), [](std::wstring left, std::wstring right) {return left.size() < right.size(); });

}

void LilSyncy::PerformSync()
{
    // NOTE: Syncing could probably be multithreaded in a similar fashion to the FileWalker but to avoid
    // hitting IO bottlenecks I would have probably set the thread count quite low so I left it single threaded for now.

    // Create folders in destination first
    for (std::wstring CurrentFolderPath : FoldersToCreate)
    {
        if (Options.DryRun || CreateDirectory(CurrentFolderPath.c_str(), NULL))
        {
            LilSyncy::LogMessage(GREEN, TEXT("[COPY] %s %s\n"),
                CurrentFolderPath.c_str(),
                Options.DryRun ? L"(dryrun)" : L"");

            ItemsCopied++;
        }
        else
        {
            LilSyncy::LogMessage(RED, TEXT("[COPY] Could not add folder %s: %s [%d] \n"),
                CurrentFolderPath.c_str(),
                GetLastErrorAsString().c_str(),
                GetLastError());

            Errors++;
        }
    }

    const size_t SourceRootPathLength = Options.SourcePath.size();
    std::wstring SourceFilePath, DestinationFilePath;
    OperationCount = SyncInstructions.size();
    while (SyncInstructions.empty() == false)
    {
        OperationsPerformed++;

        // Get next instruction
        const std::tuple<SyncInstruction, std::wstring> instruction = SyncInstructions.front();
        SyncInstructions.pop();

        // Construct source and destination file paths using the relative paths
        // (We don't need the source path for delete options but hey ho)
        SourceFilePath = Options.SourcePath + instruction._Get_rest()._Myfirst._Val;
        DestinationFilePath = SourceFilePath;
        DestinationFilePath.erase(0, SourceRootPathLength);
        DestinationFilePath.insert(0, Options.DestinationPath);

        switch (instruction._Myfirst._Val)
        {
        case COPY:
        case REPLACE:
            CurrentOperation.Filename = instruction._Get_rest()._Myfirst._Val;
            CurrentOperation.Operation = instruction._Myfirst._Val;

            if (Options.DryRun || CopyFileEx(SourceFilePath.c_str(), DestinationFilePath.c_str(), &CopyFileProgress, this, FALSE, 0))
            {
                // TODO: Would love to see file size here
                if (instruction._Myfirst._Val == COPY)
                {
                    LilSyncy::LogMessage(GREEN, TEXT("[%lu/%lu] [ADD] %s %s\n"),
                        OperationsPerformed,
                        OperationCount,
                        instruction._Get_rest()._Myfirst._Val.c_str(),
                        Options.DryRun ? L"(dryrun)" : L"");
                }
                else
                {
                    LilSyncy::LogMessage(YELLOW, TEXT("[%lu/%lu] [REPLACE] %s %s\n"),
                        OperationsPerformed,
                        OperationCount,
                        instruction._Get_rest()._Myfirst._Val.c_str(),
                        Options.DryRun ? L"(dryrun)" : L"");
                }

                ItemsCopied++;
            }
            else
            {
                LilSyncy::LogMessage(RED, TEXT("[%lu/%lu] [ERROR] Could not copy %s: %s \n"),
                    OperationsPerformed,
                    OperationCount,
                    instruction._Get_rest()._Myfirst._Val.c_str(),
                    GetLastErrorAsString().c_str());

                Errors++;
            }

            break;

        case REMOVE:

            if (Options.DryRun || DeleteFile(DestinationFilePath.c_str()))
            {
                LilSyncy::LogMessage(MAGENTA, TEXT("[%lu/%lu] [REMOVE] %s %s \n"),
                    OperationsPerformed,
                    OperationCount,
                    instruction._Get_rest()._Myfirst._Val.c_str(),
                    Options.DryRun ? L"(dryrun)" : L"");

                ItemsDeleted++;
            }
            else
            {
                LilSyncy::LogMessage(RED, TEXT("[%lu/%lu] [ERROR] Could not delete %s: %s \n"),
                    OperationsPerformed,
                    OperationCount,
                    instruction._Get_rest()._Myfirst._Val.c_str(),
                    GetLastErrorAsString().c_str());

                Errors++;
            }

            break;
        }
    }

    // Clean up any directories
    // Next go through and remove Empty/removed folders from the destination
    for (const std::wstring& CurrentFolderPath : FoldersToDelete)
    {
        if (Options.DryRun || RemoveDirectory(CurrentFolderPath.c_str()))
        {
            LilSyncy::LogMessage(MAGENTA, TEXT("[REMOVE] %s %s \n"),
                CurrentFolderPath.c_str(),
                Options.DryRun ? L"(dryrun)" : L"");

            ItemsDeleted++;
        }
        else
        {
            LilSyncy::LogMessage(RED, TEXT("[REMOVE] Error %s %s \n"),
                CurrentFolderPath.c_str(),
                GetLastErrorAsString().c_str());

            Errors++;
        }
    }
}

DWORD LilSyncy::CopyFileProgress(LARGE_INTEGER TotalFileBytes, LARGE_INTEGER TotalBytesTransferred, LARGE_INTEGER StreamSize, LARGE_INTEGER StreamSizeTransferred, DWORD dwStreamNumber, DWORD dwCallbackReason, HANDLE hSourceFile, HANDLE hDestinationFile, LPVOID lpData)
{
    const double PercentComplete = (static_cast<double>(TotalBytesTransferred.QuadPart) / static_cast<double>(TotalFileBytes.QuadPart)) * 100.0;

    LilSyncy* Self = static_cast<LilSyncy*>(lpData);

    FileOperation Operation = Self->GetCurrentOperation();

    // TODO: Move into main class
    LilSyncy::LogMessage(GREEN, TEXT("[%lu/%lu] [ADD] %s (%%%.0f)\r"),
        Self->GetProcessedInstructions(),
        Self->GetTotalInstructions(),
        Operation.Filename.c_str(),
        PercentComplete);

    return PROGRESS_CONTINUE;
}

void LilSyncy::Cleanup()
{
    Options.DestinationPath = std::wstring();
    Options.DestinationPath = std::wstring();
    Options.DryRun = false;

    ItemsCopied = 0;
    ItemsDeleted = 0;
    Errors = 0;

    FoldersToCreate.clear();
    FoldersToDelete.clear();
    std::queue<std::tuple<SyncInstruction, std::wstring>> empty;
    std::swap(SyncInstructions, empty);

    CloseHandle(ConsoleHandle);
}

// From: https://stackoverflow.com/a/17387176
std::wstring LilSyncy::GetLastErrorAsString()
{
    DWORD ErrorId = GetLastError();
    
    // Check there actually is a valid error Message
    if (ErrorId == ERROR_SUCCESS)
    {
        return std::wstring();
    }

    // Ask winapi to give us the string version of that error Message and put it in a buffer, which we then convert back to a string
    LPWCH MessageBuffer = NULL;
    size_t size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, ErrorId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWCH)&MessageBuffer, 0, NULL);
    std::wstring Message(MessageBuffer, size);
    LocalFree(MessageBuffer);

    // TODO: Could change this to save directly to a reference if performance becomes more of a factor
    return Message;
}

static CONSOLE_SCREEN_BUFFER_INFO ConsoleScreenBufferInfo;
static WORD CurrentConsoleColor;
void LilSyncy::LogMessage(ConsoleColors color, const wchar_t* format, ...)
{
    // Get current console values if they haven't been set yet
    if (ConsoleHandle == NULL)
    {
        ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        if (GetConsoleScreenBufferInfo(ConsoleHandle, &ConsoleScreenBufferInfo))
        {
            CurrentConsoleColor = ConsoleScreenBufferInfo.wAttributes;
        }
        else
        {
            // Default colour if we couldn't get the current console buffer
            CurrentConsoleColor = 8;
        }
    }

    SetConsoleTextAttribute(ConsoleHandle, color);

    va_list args;
    va_start(args, format);
    vfwprintf(stdout, format, args);
    va_end(args);

    // Cleanup the colour after
    SetConsoleTextAttribute(ConsoleHandle, CurrentConsoleColor);
}

bool LilSyncy::DoesDirectoryExist(std::wstring& pathToDirectory)
{
    const DWORD fileType = GetFileAttributes(pathToDirectory.c_str());

    if (fileType == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    return fileType & FILE_ATTRIBUTE_DIRECTORY;
}