// LilSyncy.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>
#include <tchar.h>

#include <strsafe.h>

void PrintDirectory(std::wstring _path);

int main()
{
    //const char* path = "C:\\*";
    //LPCWSTR path = L"C:\\*";
    std::wstring path = TEXT("C:\\");
    
    WIN32_FIND_DATA FoundFileData;
    HANDLE hFind;


    hFind = FindFirstFile((path + L"*").c_str(), &FoundFileData);
    do
    {
        if (FoundFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            _tprintf(TEXT("[ ] %s\n"), FoundFileData.cFileName);
            PrintDirectory(path + L"\\" + FoundFileData.cFileName + L"\\*");

        }
        else
        {
            _tprintf(TEXT("-> %s\n"), FoundFileData.cFileName);
        }
    } 
    while (FindNextFile(hFind, &FoundFileData));

    FindClose(hFind);

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
