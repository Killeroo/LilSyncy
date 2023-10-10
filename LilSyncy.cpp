// LilSyncy.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>
#include <tchar.h>
#include <queue>
#include <locale>
#include <codecvt>
#include <string>

#include "FileWalker.h"

#include <map>
#include <mutex>
#include <condition_variable>
#include <sstream>


struct Options
{
    std::wstring SourcePath;
    std::wstring DestinationPath;

    bool DryRun;
};

bool IsValidFolder(const std::wstring& path)
{
    DWORD fileType = GetFileAttributes(path.c_str());

    if (fileType == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    return fileType & FILE_ATTRIBUTE_DIRECTORY;
}

// https://stackoverflow.com/a/18597384
std::wstring ToWideString(char*& string)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(string);
}

void ParseArguments(int argc, wchar_t* argv[], Options& outOptions)
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
                if (IsValidFolder(nextArgument))
                {
                    // Sometimes we can get valid path without the closing slash
                    // (winapi won't like this so lets add it in ourselves)
                    if (nextArgument.back() != '\\')
                    {
                        nextArgument.push_back('\\');
                    }

                    outOptions.SourcePath = nextArgument;
                }
            }
        }
        else if (argument == L"--destination")
        {
            if (nextArgument != L"")
            {
                if (IsValidFolder(nextArgument))
                {
                    if (nextArgument.back() != '\\')
                    {
                        nextArgument.push_back('\\');
                    }

                    outOptions.DestinationPath = nextArgument;
                }
            }
        }
        else if (argument == L"--dryrun")
        {
            outOptions.DryRun = true;
        }
    }
}


void Error(std::wstring message)
{
    
}


// TODO: Could be quicker? Do file creation in reverse?
void CreatePath(std::wstring& root, std::wstring& relativePathToCreate)
{

    _tprintf(TEXT("Create %s \n"), relativePathToCreate.c_str());

    std::wstringstream pathStream(relativePathToCreate.c_str());
    std::wstring folder;
    std::vector<std::wstring> folders;

    while (std::getline(pathStream, folder, L'\\'))
    {
        folders.push_back(folder);
    }

    std::wstring currentPath = root;
    for (int i = 0; i < folders.size(); i++)
    {
        currentPath.append(L"\\").append(folders[i]);

        if (CreateDirectory(currentPath.c_str(), NULL) == false)
        {
            _tprintf(TEXT("Couldn't create %s \n"), currentPath.c_str());
        }

    }
}

bool DoesDirectoryExist(std::wstring& pathToDirectory)
{
    DWORD fileType = GetFileAttributes(pathToDirectory.c_str());

    if (fileType == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    return fileType & FILE_ATTRIBUTE_DIRECTORY;
}

enum Instruction : byte
{
    COPY,
    REPLACE,
    REMOVE,
};

bool StringSortMethodDescendingSize(std::wstring left, std::wstring right)
{
    return left.size() > right.size();
}
bool StringSortMethodAscendingSize(std::wstring left, std::wstring right)
{
    return left.size() < right.size();
}

int wmain(int argc, wchar_t* argv[])
{
    Options parsedOptions;
    parsedOptions.DryRun = false;
    ParseArguments(argc, argv, parsedOptions);

    if (parsedOptions.DestinationPath == L"" && parsedOptions.SourcePath == L"")
        //|| parsedOptions.DestinationPath == parsedOptions.SourcePath)
    {
        // TODO: Error message
        return 1;
    }

    //const char* path = "C:\\*";
    //LPCWSTR path = L"C:\\*";
    //std::wstring path = TEXT("C:\\Projects\\");// TEXT("C:\\");

    // Gather files
    FileWalker walker;
    std::map<std::wstring, FileData> sourceFiles = walker.GetFiles(parsedOptions.SourcePath);
    std::map<std::wstring, FileData> destinationFiles = walker.GetFiles(parsedOptions.DestinationPath);

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

                _tprintf(TEXT("Missing file %s \n"), entry.first.c_str());
            }
            else
            {

                FoldersToCreate.push_back(parsedOptions.DestinationPath + entry.first);

                _tprintf(TEXT("Missing folder %s \n"), entry.first.c_str());
            }

            continue;
        }

        // Next check to see if the sizes match
        if (destinationFiles[entry.first].Size != entry.second.Size)
        {
            if (entry.second.Directory == false)
            {
                Instructions.enqueue(std::make_tuple(REPLACE, entry.first));

                _tprintf(TEXT("Different length for %s \n"), entry.first.c_str());
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

    // Sort the folders lists descending then ascending
    // This ensures that we delete child directories before attempting to delete their parents
    // and ensures that the files 
    // =================================
    // JUST WRITE A COMMENT HERE
    // =================================
    std::sort(FoldersToDelete.begin(), FoldersToDelete.end(), StringSortMethodDescendingSize);
    std::sort(FoldersToCreate.begin(), FoldersToCreate.end(), StringSortMethodAscendingSize);

    //parsedOptions.DestinationPath.erase(parsedOptions.DestinationPath.size() - 1, 1);

    // Create folders in destination first
    for (std::wstring currentFolder : FoldersToCreate)
    {

        if (CreateDirectory(currentFolder.c_str(), NULL))
        {
            _tprintf(TEXT("[CREATE FOLDER] %s %s \n"),
                currentFolder.c_str(),
                parsedOptions.DryRun ? L"(dryrun)" : L"");
        }
        else
        {
            _tprintf(TEXT("[CREATE FOLDER] Error %s %d \n"),
                currentFolder.c_str(),
                GetLastError());
        }
    }

    std::wstring fullSourcePath, fullDestinationPath;
    bool result = false;
    const size_t sourceRootPathLength = parsedOptions.SourcePath.size();
    while (Instructions.empty() == false)
    {
        const std::tuple<Instruction, std::wstring> instruction = Instructions.dequeue();

        // We also don't need to do this for delete options but hey ho
        const std::wstring sourcePath = parsedOptions.SourcePath + instruction._Get_rest()._Myfirst._Val;
        

        std::wstring destinationFilePath = sourcePath;// sourceFiles[instruction._Get_rest()._Myfirst._Val].Path;
        destinationFilePath.erase(0, sourceRootPathLength);
        destinationFilePath.insert(0, parsedOptions.DestinationPath);


        switch (instruction._Myfirst._Val)
        {
        case COPY:
        case REPLACE:
        {

            // Use CopyFileEx for progress
            //if (parsedOptions.DryRun || CopyFile(sourcePath.c_str(), destinationFilePath.c_str(), false))


            std::wstring destinationPath = sourceFiles[instruction._Get_rest()._Myfirst._Val].Path;
            destinationPath.erase(0, sourceRootPathLength);
            std::wstring relativePath = destinationPath;
            destinationPath.insert(0, parsedOptions.DestinationPath);

            // Not required anymore
            //if (DoesDirectoryExist(destinationPath) == false)
            //{
            //    CreatePath(parsedOptions.DestinationPath, relativePath);
            //}

            if (CopyFile(sourcePath.c_str(), destinationFilePath.c_str(), false))
            {
                _tprintf(TEXT("[COPY] %s %s \n"), 
                    instruction._Get_rest()._Myfirst._Val.c_str(),
                    parsedOptions.DryRun ? L"(dryrun)" : L"");
            }
            else
            {
                _tprintf(TEXT("[ERROR] Could not copy %s: %d %s\n"),
                    instruction._Get_rest()._Myfirst._Val.c_str(),
                    GetLastError(),
                    parsedOptions.DryRun ? L"(dryrun)" : L"");
            }

            break;
        }

        case REMOVE:
        {


            if (DeleteFile(destinationFilePath.c_str()))
            {
                _tprintf(TEXT("[REMOVE] %s %s \n"),
                    instruction._Get_rest()._Myfirst._Val.c_str(),
                    parsedOptions.DryRun ? L"(dryrun)" : L"");
            }

            break;
        }
        }

        // Add - Green
        // Replace - yellow
        // Remove - red
    }
    

    // Clean up any directories
    // Next go through and remove empty/removed folders from the destination
    for (std::wstring currentFolder : FoldersToDelete)
    {

        if (RemoveDirectory(currentFolder.c_str()))
        {
            _tprintf(TEXT("[REMOVE FOLDER] %s %s \n"),
                currentFolder.c_str(),
                parsedOptions.DryRun ? L"(dryrun)" : L"");
        }
        else
        {
            _tprintf(TEXT("[REMOVE FOLDER] Error %s %d \n"),
                currentFolder.c_str(),
                GetLastError());
        }
    }

    return 0;
}