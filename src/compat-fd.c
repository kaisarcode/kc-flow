/**
 * compat-fd.c
 * Summary: Descriptor and artifact platform primitives for kc-flow.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#define _POSIX_C_SOURCE 200809L

#include "compat.h"

#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <process.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

/**
 * Resolves the current process id.
 * @return long Current process id.
 */
long kc_flow_platform_pid(void) {
#if defined(_WIN32)
    return (long)_getpid();
#else
    return (long)getpid();
#endif
}

/**
 * Writes one exact byte range.
 * @param fd Destination descriptor.
 * @param buffer Source buffer.
 * @param size Requested size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_platform_write_all(int fd, const void *buffer, size_t size) {
    size_t offset;

    offset = 0;
    while (offset < size) {
#if defined(_WIN32)
        int rc;

        rc = _write(fd, (const char *)buffer + offset, (unsigned int)(size - offset));
        if (rc <= 0) {
            return -1;
        }
#else
        ssize_t rc;

        rc = write(fd, (const char *)buffer + offset, size - offset);
        if (rc < 0 && errno == EINTR) {
            continue;
        }
        if (rc <= 0) {
            return -1;
        }
#endif
        offset += (size_t)rc;
    }
    return 0;
}

/**
 * Rewinds one descriptor to its start.
 * @param fd Descriptor.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_platform_rewind_fd(int fd) {
#if defined(_WIN32)
    return _lseek(fd, 0, SEEK_SET) < 0 ? -1 : 0;
#else
    return lseek(fd, 0, SEEK_SET) < 0 ? -1 : 0;
#endif
}

/**
 * Duplicates one descriptor.
 * @param fd Source descriptor.
 * @return int Duplicated descriptor or -1.
 */
int kc_flow_platform_dup_fd(int fd) {
#if defined(_WIN32)
    return _dup(fd);
#else
    return dup(fd);
#endif
}

/**
 * Closes one descriptor.
 * @param fd Descriptor.
 * @return void
 */
void kc_flow_platform_close_fd(int fd) {
    if (fd < 0) {
        return;
    }
#if defined(_WIN32)
    _close(fd);
#else
    close(fd);
#endif
}

/**
 * Opens one null descriptor for fallback I/O.
 * @param write_mode Non-zero for write access.
 * @return int Descriptor or -1.
 */
int kc_flow_platform_open_null_fd(int write_mode) {
#if defined(_WIN32)
    return _open("NUL", write_mode ? _O_WRONLY : _O_RDONLY);
#else
    return open("/dev/null", write_mode ? O_WRONLY : O_RDONLY);
#endif
}

/**
 * Creates one anonymous artifact descriptor.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int Descriptor or -1.
 */
int kc_flow_platform_create_artifact_fd(char *error, size_t error_size) {
#if defined(_WIN32)
    FILE *fp;

    fp = tmpfile();
    if (fp == NULL) {
        snprintf(error, error_size, "Unable to create runtime artifact.");
        return -1;
    }
    return _dup(_fileno(fp));
#else
    char path[] = "/tmp/kc-flow-XXXXXX";
    int fd;

    fd = mkstemp(path);
    if (fd < 0) {
        snprintf(error, error_size, "Unable to create runtime artifact.");
        return -1;
    }
    unlink(path);
    return fd;
#endif
}
