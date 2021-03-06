//
// sliceIO.cc
//
// Copyright © 2018 Couchbase. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "sliceIO.hh"

#if FL_HAVE_FILESYSTEM

#include "FleeceException.hh"
#include "PlatformCompat.hh"
#include <fcntl.h>
#include <errno.h>

#ifndef _MSC_VER
    #include <sys/stat.h>
    #include <unistd.h>
#else
    #include <io.h>
    #include <windows.h>
#endif

#ifndef _MSC_VER
#define O_BINARY 0
#endif


namespace fleece {

    alloc_slice readFile(const char *path) {
        int fd = ::open(path, O_RDONLY | O_BINARY);
        if (fd < 0)
            FleeceException::_throwErrno("Can't open file");
        struct stat stat;
        fstat(fd, &stat);
        if (stat.st_size > SIZE_MAX)
            throw std::logic_error("File too big for address space");
        alloc_slice data((size_t)stat.st_size);
        ssize_t bytesRead = ::read(fd, (void*)data.buf, data.size);
        if (bytesRead < (ssize_t)data.size)
            FleeceException::_throwErrno("Can't read file");
        ::close(fd);
        return data;
    }

    void writeToFile(slice s, const char *path, int mode) {
        int fd = ::open(path, mode | O_WRONLY | O_BINARY, 0600);
        if (fd < 0)
            FleeceException::_throwErrno("Can't open file");
        ssize_t written = ::write(fd, s.buf, s.size);
        if(written < (ssize_t)s.size)
            FleeceException::_throwErrno("Can't write file");
        ::close(fd);
    }

    void writeToFile(slice s, const char *path) {
        writeToFile(s, path, O_CREAT | O_TRUNC);
    }


    void appendToFile(slice s, const char *path) {
        writeToFile(s, path, O_CREAT | O_APPEND);
    }

}

#endif // FL_HAVE_FILESYSTEM
