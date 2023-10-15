#pragma once

constexpr float VERSION = 1.0f;

constexpr int FILE_WALKER_THREAD_COUNT = 10;

constexpr const char* USAGE_TEXT =
"A small and relatively fast tool that syncs folders and files from one folder to another.\n\
\n\
Usage: \n\
[required] --source           Path you want to sync from.\n\
[required] --destination      Path you want to sync to.\n\
\n\
           --dryrun           Run sync without actually moving or deleting any files.\n\
           --help             Displays this help message.\n\
\n\
Written by Matthew Carney [matthewcarney64@gmail.com]\n\
Find the project here [https://github.com/Killeroo/LilSyncy]";