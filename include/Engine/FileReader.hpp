#pragma once

#include "Engine/Log.hpp"

#include <cstdio>
#include <cerrno>

class FileReader
{
protected:
    char* buffer = nullptr;

public:
    FileReader(const char* filename)
    {
        size_t     string_size, read_size;
        FILE*   handler;
        int err = 0;

#ifdef _WIN32
        err = fopen_s(&handler, filename, "rb");
#else
        handler = fopen(filename, "rb");
        err = handler ? 0 : errno;
#endif
        if (err != 0)
        {
            logf("The file '%s' was not opened\n", filename);
        }

        if (handler)
        {
            // Seek the last byte of the file
            fseek(handler, 0, SEEK_END);
            // Offset from the first to the last byte, or in other words, filesize
            string_size = ftell(handler);
            // go back to the start of the file
            rewind(handler);

            // Allocate a string that can hold it all
            buffer = (char*)malloc(sizeof(char) * (string_size + 1));

            // Read it all in one operation
            read_size = fread(buffer, sizeof(char), string_size, handler);

            // fread doesn't set it so put a \0 in the last position
            // and buffer is now officially a string
            buffer[string_size] = '\0';

            if (string_size != read_size)
            {
                // Something went wrong, throw away the memory and set
                // the buffer to NULL
                free(buffer);
                buffer = NULL;
            }

            // Always remember to close the file.
            fclose(handler);
        }
    }

    ~FileReader()
    {
        free(buffer);
        buffer = NULL;
    }

    const char* get()
    {
        return buffer;
    }
};
