/*
 * Copyright 1993-2015 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

// These are helper functions for the SDK samples (string parsing, timers, etc)
#ifndef STRING_HELPER_H
#define STRING_HELPER_H

#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <string>

#if defined(WIN32) || defined(_WIN32) || defined(WIN64)
  #ifndef _CRT_SECURE_NO_DEPRECATE
  #define _CRT_SECURE_NO_DEPRECATE
  #endif
  #ifndef STRCASECMP
  #define STRCASECMP  _stricmp
  #endif
  #ifndef STRNCASECMP
  #define STRNCASECMP _strnicmp
  #endif
  #ifndef STRCPY
  #define STRCPY(sFilePath, nLength, sPath) strcpy_s(sFilePath, nLength, sPath)
  #endif

  #ifndef FOPEN
  #define FOPEN(fHandle,filename,mode) fopen_s(&fHandle, filename, mode)
  #endif
  #ifndef FOPEN_FAIL
  #define FOPEN_FAIL(result) (result != 0)
  #endif
  #ifndef SSCANF
  #define SSCANF sscanf_s
  #endif
#else
  #include <string.h>
  #include <strings.h>

  #ifndef STRCASECMP
  #define STRCASECMP  strcasecmp
  #endif
  #ifndef STRNCASECMP
  #define STRNCASECMP strncasecmp
  #endif
  #ifndef STRCPY
  #define STRCPY(sFilePath, nLength, sPath) strcpy(sFilePath, sPath)
  #endif

  #ifndef FOPEN
  #define FOPEN(fHandle,filename,mode) (fHandle = fopen(filename, mode))
  #endif
  #ifndef FOPEN_FAIL
  #define FOPEN_FAIL(result) (result == NULL)
  #endif
  #ifndef SSCANF
  #define SSCANF sscanf
  #endif
#endif

#ifndef EXIT_WAIVED
#define EXIT_WAIVED 2
#endif

// CUDA Utility Helper Functions
inline int stringRemoveDelimiter(char delimiter, const char *string)
{
    int string_start = 0;

    while (string[string_start] == delimiter)
    {
        string_start++;
    }

    if (string_start >= (int)strlen(string)-1)
    {
        return 0;
    }

    return string_start;
}

inline int getFileExtension(char *filename, char **extension)
{
    int string_length = (int)strlen(filename);

    while (filename[string_length--] != '.') {
        if (string_length == 0)
            break;
    }
    if (string_length > 0) string_length += 2;

    if (string_length == 0) 
        *extension = NULL;
    else 
        *extension = &filename[string_length];

    return string_length;
}


inline int checkCmdLineFlag(const int argc, const char **argv, const char *string_ref)
{
    bool bFound = false;

    if (argc >= 1)
    {
        for (int i=1; i < argc; i++)
        {
            int string_start = stringRemoveDelimiter('-', argv[i]);
            const char *string_argv = &argv[i][string_start];

            const char *equal_pos = strchr(string_argv, '=');
            int argv_length = (int)(equal_pos == 0 ? strlen(string_argv) : equal_pos - string_argv);

            int length = (int)strlen(string_ref);

            if (length == argv_length && !STRNCASECMP(string_argv, string_ref, length))
            {

                bFound = true;
                continue;
            }
        }
    }

    return (int)bFound;
}

// This function wraps the CUDA Driver API into a template function
template <class T>
inline bool getCmdLineArgumentValue(const int argc, const char **argv, const char *string_ref, T *value)
{
    bool bFound = false;
    if (argc >= 1) {
        for (int i=1; i < argc; i++) {
            int string_start = stringRemoveDelimiter('-', argv[i]);
            const char *string_argv = &argv[i][string_start];
            int length = (int)strlen(string_ref);
            if (!STRNCASECMP(string_argv, string_ref, length) ) {
                if (length+1 <= (int)strlen(string_argv)) {
                    int auto_inc = (string_argv[length] == '=') ? 1 : 0;
                    *value = (T)atoi(&string_argv[length + auto_inc]);
                }
                bFound = true;
                i=argc;
            }
        }
    }
    return bFound;
}

inline int getCmdLineArgumentInt(const int argc, const char **argv, const char *string_ref)
{
    bool bFound = false;
    int value = -1;

    if (argc >= 1)
    {
        for (int i=1; i < argc; i++)
        {
            int string_start = stringRemoveDelimiter('-', argv[i]);
            const char *string_argv = &argv[i][string_start];
            int length = (int)strlen(string_ref);

            if (!STRNCASECMP(string_argv, string_ref, length))
            {
                if (length+1 <= (int)strlen(string_argv))
                {
                    int auto_inc = (string_argv[length] == '=') ? 1 : 0;
                    value = atoi(&string_argv[length + auto_inc]);
                }
                else
                {
                    value = 0;
                }

                bFound = true;
                continue;
            }
        }
    }

    if (bFound)
    {
        return value;
    }
    else
    {
        return 0;
    }
}

inline float getCmdLineArgumentFloat(const int argc, const char **argv, const char *string_ref)
{
    bool bFound = false;
    float value = -1;

    if (argc >= 1)
    {
        for (int i=1; i < argc; i++)
        {
            int string_start = stringRemoveDelimiter('-', argv[i]);
            const char *string_argv = &argv[i][string_start];
            int length = (int)strlen(string_ref);

            if (!STRNCASECMP(string_argv, string_ref, length))
            {
                if (length+1 <= (int)strlen(string_argv))
                {
                    int auto_inc = (string_argv[length] == '=') ? 1 : 0;
                    value = (float)atof(&string_argv[length + auto_inc]);
                }
                else
                {
                    value = 0.f;
                }

                bFound = true;
                continue;
            }
        }
    }

    if (bFound)
    {
        return value;
    }
    else
    {
        return 0;
    }
}

inline bool getCmdLineArgumentString(const int argc, const char **argv,
                                     const char *string_ref, char **string_retval)
{
    bool bFound = false;

    if (argc >= 1)
    {
        for (int i=1; i < argc; i++)
        {
            int string_start = stringRemoveDelimiter('-', argv[i]);
            char *string_argv = (char *)&argv[i][string_start];
            int length = (int)strlen(string_ref);

            if (!STRNCASECMP(string_argv, string_ref, length))
            {
                *string_retval = &string_argv[length+1];
                bFound = true;
                continue;
            }
        }
    }

    if (!bFound)
    {
        *string_retval = NULL;
    }

    return bFound;
}

//////////////////////////////////////////////////////////////////////////////
//! Find the path for a file assuming that
//! files are found in the searchPath.
//!
//! @return the path if succeeded, otherwise 0
//! @param filename         name of the file
//! @param executable_path  optional absolute path of the executable
//////////////////////////////////////////////////////////////////////////////
inline char *sdkFindFilePath(const char *filename, const char *executable_path)
{
    // <executable_name> defines a variable that is replaced with the name of the executable

    // Typical relative search paths to locate needed companion files (e.g. sample input data, or JIT source files)
    // The origin for the relative search may be the .exe file, a .bat file launching an .exe, a browser .exe launching the .exe or .bat, etc
    const char *searchPath[] =
    {
        "./",                                       // same dir
        "./data/",                                  // "/data/" subdir
        "./<executable_name>/data/",                // "/<executable_name>/data/" subdir
        "./inc/",                                   // "/inc/" subdir
        "./common/video/",                          // "/common/video/" subdir
        "./common/kernels/ptx/",

        "../",                                      // up 1 in tree
        "../data/",                                 // up 1 in tree, "/data/" subdir
        "../<executable_name>/data/",               // up 1 in tree, "/<executable_name>/data/" subdir
        "../inc/",                                  // up 1 in tree, "/inc/" subdir
        "../common/video/",                         // up 1 in tree, "/common/video/" subdir
        "../common/kernels/ptx/",

        "../../",                                   // up 2 in tree
        "../../data/",                              // up 2 in tree, "/data/" subdir
        "../../<executable_name>/data/",            // up 2 in tree, "/<executable_name>/data/" subdir
        "../../inc/",                               // up 2 in tree, "/inc/" subdir
        "../../common/video/",                      // up 2 in tree, "/common/video/" subdir
        "../../common/kernels/ptx/",

        "../../../",                                // up 3 in tree
        "../../../data/",                           // up 3 in tree, "../../../data/" subdir
        "../../../<executable_name>/data/",         // up 3 in tree, "/<executable_name>/data/" subdir
        "../../../common/video/",                   // up 3 in tree, "/common/video/" subdir
        "../../../common/kernels/ptx/",

        "../../../../",                             // up 4 in tree
        "../../../../data/",                        // up 4 in tree, "../../../data/" subdir    
        "../../../../<executable_name>/data/",      // up 4 in tree, "/<executable_name>/data/" subdir
        "../../../../common/video/",                // up 4 in tree, "/common/video/" subdir
        "../../../../common/kernels/ptx/",
    };

    // Extract the executable name
    std::string executable_name;

    if (executable_path != 0)
    {
        executable_name = std::string(executable_path);

#ifdef _WIN32
        // Windows path delimiter
        size_t delimiter_pos = executable_name.find_last_of('\\');
        executable_name.erase(0, delimiter_pos + 1);

        if (executable_name.rfind(".exe") != std::string::npos)
        {
            // we strip .exe, only if the .exe is found
            executable_name.resize(executable_name.size() - 4);
        }

#else
        // Linux & OSX path delimiter
        size_t delimiter_pos = executable_name.find_last_of('/');
        executable_name.erase(0,delimiter_pos+1);
#endif
    }

    // Loop over all search paths and return the first hit
    for (unsigned int i = 0; i < sizeof(searchPath)/sizeof(char *); ++i)
    {
        std::string path(searchPath[i]);
        size_t executable_name_pos = path.find("<executable_name>");

        // If there is executable_name variable in the searchPath
        // replace it with the value
        if (executable_name_pos != std::string::npos)
        {
            if (executable_path != 0)
            {
                path.replace(executable_name_pos, strlen("<executable_name>"), executable_name);
            }
            else
            {
                // Skip this path entry if no executable argument is given
                continue;
            }
        }

#ifdef _DEBUG
//        printf("sdkFindFilePath <%s> in %s\n", filename, path.c_str());
#endif

        // Test if the file exists
        path.append(filename);
        FILE *fp;
        FOPEN(fp, path.c_str(), "rb");

        if (fp != NULL)
        {
            fclose(fp);
            // File found
            // returning an allocated array here for backwards compatibility reasons
            char *file_path = (char *) malloc(path.length() + 1);
            STRCPY(file_path, path.length() + 1, path.c_str());
            return file_path;
        }

        if (fp)
        {
            fclose(fp);
        }
    }

    // File not found
    return 0;
}

#endif
