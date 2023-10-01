#pragma once

#include <Windows.h>
#include <string>

struct FileData
{
    std::wstring Name;
    std::wstring Path;
    int64_t Size;
    FILETIME LastWriteTime;
};