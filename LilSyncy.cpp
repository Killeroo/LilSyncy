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
#include <format>
#include <string_view>
#include <map>
#include <mutex>
#include <condition_variable>
#include <sstream>

#include "FileWalker.h"
#include "LilSyncy.h"




int LilSyncy::Run(int argc, wchar_t* argv[])
{
    ParseArguments(argc, argv);

    if ((Options.DestinationPath == L"" || Options.SourcePath == L"")
        || Options.DestinationPath == Options.SourcePath)
    {
        // TODO: Error message
        LilSyncy::LogMessage(RED, TEXT("Could not find valid source and destination folders. Please check and try again."));
        return 1;
    }

    // Gather files
    FileWalker walker;
    std::map<std::wstring, FileData> sourceFiles = walker.GetFiles(Options.SourcePath);
    std::map<std::wstring, FileData> destinationFiles = walker.GetFiles(Options.DestinationPath);

    CalculateFolderDifferences(sourceFiles, destinationFiles);
    PerformSync();

    // TODO: Cleanup
    //SyncInstructions
    FoldersToCreate.clear();
    FoldersToDelete.clear();

    CloseHandle(ConsoleHandle);

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
    }
}

void LilSyncy::CalculateFolderDifferences(std::map<std::wstring, FileData>& sourceFiles, std::map<std::wstring, FileData>& destinationFiles)
{
    // Ok lets work out what files we need to sync
    // We loop through the sources files as they are our 'source of truth'
    SafeQueue<std::tuple<Instruction, std::wstring>> Instructions;
    std::vector<std::wstring> FoldersToCreate;
    for (auto const& entry : sourceFiles)
    {
        // First check if the file is missing in the destination.
        // Copy it if so.
        if (destinationFiles.count(entry.first) == 0)
        {
            if (entry.second.Directory == false)
            {
                Instructions.enqueue(std::make_tuple(COPY, entry.first));
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
                Instructions.enqueue(std::make_tuple(REPLACE, entry.first));
                continue;
            }
        }

        // TODO: Check last write time..

        // TODO: Check file signature..
    }

    // Save files and folders that we need to remove from the destination
    // We store files in a seperate list because we want to remove them after we have deleted the files that
    // were in them
    std::vector<std::wstring> FoldersToDelete;
    for (auto const& entry : destinationFiles)
    {

        // Remove any files in the destination that don't exist in the source
        if (sourceFiles.count(entry.first) == 0)
        {
            if (entry.second.Directory == false)
            {
                Instructions.enqueue(std::make_tuple(REMOVE, entry.first));
                _tprintf(TEXT("Remove file %s \n"), entry.first.c_str());
            }
            else
            {
                std::wstring currentDir = entry.second.Path + entry.second.Name;
                FoldersToDelete.push_back(currentDir);
                _tprintf(TEXT("Remove folder %s \n"), currentDir.c_str());
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
    // Create folders in destination first
    for (std::wstring currentFolder : FoldersToCreate)
    {
        if (Options.DryRun || CreateDirectory(currentFolder.c_str(), NULL))
        {
            _tprintf(TEXT("[COPY] %s %s \n"),
                currentFolder.c_str(),
                parsedOptions.DryRun ? L"(dryrun)" : L"");
        }
        else
        {
            _tprintf(TEXT("[COPY] Error %s %d \n"),
                currentFolder.c_str(),
                GetLastError());
        }
    }

    std::wstring fullSourcePath, fullDestinationPath;
    bool result = false;
    const size_t sourceRootPathLength = Options.SourcePath.size();
    std::wstring sourceFilePath, destinationFilePath;
    while (Instructions.empty() == false)
    {
        const std::tuple<Instruction, std::wstring> instruction = Instructions.dequeue();

        // Construct source and destination file paths using the relative paths
        // (We don't need the source path for delete options but hey ho)
        sourceFilePath = Options.SourcePath + instruction._Get_rest()._Myfirst._Val;
        destinationFilePath = sourceFilePath;
        destinationFilePath.erase(0, sourceRootPathLength);
        destinationFilePath.insert(0, parsedOptions.DestinationPath);

        switch (instruction._Myfirst._Val)
        {
        case COPY:
        case REPLACE:

            if (parsedOptions.DryRun || CopyFile(sourceFilePath.c_str(), destinationFilePath.c_str(), false))
            {
                if (instruction._Myfirst._Val == COPY)
                {
                    LilSyncy::LogMessage(GREEN, TEXT("[ADD] %s %s \n"),
                        instruction._Get_rest()._Myfirst._Val.c_str(),
                        parsedOptions.DryRun ? L"(dryrun)" : L"");
                }
                else
                {
                    LilSyncy::LogMessage(YELLOW, TEXT("[REPLACE] %s %s \n"),
                        instruction._Get_rest()._Myfirst._Val.c_str(),
                        parsedOptions.DryRun ? L"(dryrun)" : L"");
                }
            }
            else
            {
                LilSyncy::LogMessage(RED, TEXT("[ERROR] Could not copy %s: %d \n"),
                    instruction._Get_rest()._Myfirst._Val.c_str(),
                    GetLastError());
            }

            break;

        case REMOVE:

            if (parsedOptions.DryRun || DeleteFile(destinationFilePath.c_str()))
            {
                LilSyncy::LogMessage(RED, TEXT("[REMOVE] %s %s \n"),
                    instruction._Get_rest()._Myfirst._Val.c_str(),
                    parsedOptions.DryRun ? L"(dryrun)" : L"");
            }

            break;
        }
    }

    // Clean up any directories
    // Next go through and remove empty/removed folders from the destination
    for (std::wstring currentFolder : FoldersToDelete)
    {
        if (parsedOptions.DryRun || RemoveDirectory(currentFolder.c_str()))
        {
            LilSyncy::LogMessage(RED, TEXT("[REMOVE] %s %s \n"),
                currentFolder.c_str(),
                parsedOptions.DryRun ? L"(dryrun)" : L"");
        }
        else
        {
            LilSyncy::LogMessage(RED, TEXT("[REMOVE] Error %s %d \n"),
                currentFolder.c_str(),
                GetLastError());
        }
    }
}


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

    // Reset the colour after
    SetConsoleTextAttribute(ConsoleHandle, CurrentConsoleColor);
}

bool LilSyncy::DoesDirectoryExist(std::wstring& pathToDirectory)
{
    DWORD fileType = GetFileAttributes(pathToDirectory.c_str());

    if (fileType == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    return fileType & FILE_ATTRIBUTE_DIRECTORY;
}