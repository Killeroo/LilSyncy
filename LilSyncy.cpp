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
    for (std::wstring currentFolder : FoldersToCreate)
    {
        if (Options.DryRun || CreateDirectory(currentFolder.c_str(), NULL))
        {
            LilSyncy::LogMessage(GREEN, TEXT("[COPY] %s %s\n"),
                currentFolder.c_str(),
                Options.DryRun ? L"(dryrun)" : L"");

            ItemsCopied++;
        }
        else
        {
            LilSyncy::LogMessage(RED, TEXT("[COPY] Could not add folder %s: %s [%d] \n"),
                currentFolder.c_str(),
                GetLastErrorAsString().c_str(),
                GetLastError());

            Errors++;
        }
    }

    std::wstring fullSourcePath, fullDestinationPath;
    bool result = false;
    const size_t sourceRootPathLength = Options.SourcePath.size();
    std::wstring sourceFilePath, destinationFilePath;
    while (SyncInstructions.empty() == false)
    {
        // Get next instruction
        const std::tuple<Instruction, std::wstring> instruction = SyncInstructions.front();
        SyncInstructions.pop();

        // Construct source and destination file paths using the relative paths
        // (We don't need the source path for delete options but hey ho)
        sourceFilePath = Options.SourcePath + instruction._Get_rest()._Myfirst._Val;
        destinationFilePath = sourceFilePath;
        destinationFilePath.erase(0, sourceRootPathLength);
        destinationFilePath.insert(0, Options.DestinationPath);

        switch (instruction._Myfirst._Val)
        {
        case COPY:
        case REPLACE:

            if (Options.DryRun || CopyFile(sourceFilePath.c_str(), destinationFilePath.c_str(), false))
            {
                if (instruction._Myfirst._Val == COPY)
                {
                    LilSyncy::LogMessage(GREEN, TEXT("[ADD] %s %s\n"),
                        instruction._Get_rest()._Myfirst._Val.c_str(),
                        Options.DryRun ? L"(dryrun)" : L"");
                }
                else
                {
                    LilSyncy::LogMessage(YELLOW, TEXT("[REPLACE] %s %s\n"),
                        instruction._Get_rest()._Myfirst._Val.c_str(),
                        Options.DryRun ? L"(dryrun)" : L"");
                }

                ItemsCopied++;
            }
            else
            {
                LilSyncy::LogMessage(RED, TEXT("[ERROR] Could not copy %s: %s [%d] \n"),
                    instruction._Get_rest()._Myfirst._Val.c_str(),
                    GetLastErrorAsString().c_str(),
                    GetLastError());

                Errors++;
            }

            break;

        case REMOVE:

            if (Options.DryRun || DeleteFile(destinationFilePath.c_str()))
            {
                LilSyncy::LogMessage(MAGENTA, TEXT("[REMOVE] %s %s \n"),
                    instruction._Get_rest()._Myfirst._Val.c_str(),
                    Options.DryRun ? L"(dryrun)" : L"");

                ItemsDeleted++;
            }
            else
            {
                LilSyncy::LogMessage(RED, TEXT("[ERROR] Could not delete %s: %s [%d] \n"),
                    instruction._Get_rest()._Myfirst._Val.c_str(),
                    GetLastErrorAsString().c_str(),
                    GetLastError());

                Errors++;
            }

            break;
        }
    }

    // Clean up any directories
    // Next go through and remove Empty/removed folders from the destination
    for (std::wstring currentFolder : FoldersToDelete)
    {
        if (Options.DryRun || RemoveDirectory(currentFolder.c_str()))
        {
            LilSyncy::LogMessage(MAGENTA, TEXT("[REMOVE] %s %s \n"),
                currentFolder.c_str(),
                Options.DryRun ? L"(dryrun)" : L"");

            ItemsDeleted++;
        }
        else
        {
            LilSyncy::LogMessage(RED, TEXT("[REMOVE] Error %s %s \n"),
                currentFolder.c_str(),
                GetLastErrorAsString().c_str());

            Errors++;
        }
    }
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
    std::queue<std::tuple<Instruction, std::wstring>> empty;
    std::swap(SyncInstructions, empty);

    CloseHandle(ConsoleHandle);
}

// From: https://stackoverflow.com/a/17387176
std::wstring LilSyncy::GetLastErrorAsString()
{
    DWORD errorId = GetLastError();
    
    // Check there actually is a valid error message
    if (errorId == ERROR_SUCCESS)
    {
        return std::wstring();
    }

    // Ask winapi to give us the string version of that error message and put it in a buffer, which we then convert back to a string
    LPWCH messageBuffer = NULL;
    size_t size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWCH)&messageBuffer, 0, NULL);
    std::wstring message(messageBuffer, size);
    LocalFree(messageBuffer);

    // TODO: Could change this to save directly to a reference if performance becomes more of a factor
    return message;
}

static CONSOLE_SCREEN_BUFFER_INFO ConsoleScreenBufferInfo;
static WORD CurrentConsoleColor;
void LilSyncy::LogMessage(Colors color, const wchar_t* format, ...)
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