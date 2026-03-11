/**
 * artifact.c
 * Summary: Anonymous artifact descriptor helpers for kc-flow runtime transport.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#define _POSIX_C_SOURCE 200809L

#include "flow.h"
#include "compat.h"

#include <stdio.h>
#if defined(_WIN32)
#include <io.h>
#else
#include <errno.h>
#include <unistd.h>
#endif

/**
 * Copies one descriptor into another.
 * @param src Source descriptor.
 * @param dst Destination descriptor.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_copy_artifact_fd(int src, int dst) {
    char buffer[4096];
    for (;;) {
#if defined(_WIN32)
        int read_size;

        read_size = _read(src, buffer, sizeof(buffer));
        if (read_size < 0) {
            return -1;
        }
#else
        ssize_t read_size;

        read_size = read(src, buffer, sizeof(buffer));
        if (read_size < 0 && errno == EINTR) {
            continue;
        }
        if (read_size < 0) {
            return -1;
        }
#endif
        if (read_size == 0) {
            break;
        }
        if (kc_flow_platform_write_all(dst, buffer, (size_t)read_size) != 0) {
            return -1;
        }
    }
    return 0;
}

/**
 * Releases one descriptor owned by the runtime.
 * @param fd Descriptor.
 * @return void
 */
void kc_flow_release_fd(int fd) {
    if (fd < 0) {
        return;
    }
    kc_flow_platform_close_fd(fd);
}

/**
 * Creates one anonymous artifact descriptor.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int Descriptor or -1.
 */
int kc_flow_create_artifact_fd(char *error, size_t error_size) {
    return kc_flow_platform_create_artifact_fd(error, error_size);
}

/**
 * Duplicates one stored artifact descriptor.
 * @param fd Source descriptor.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int Duplicated descriptor or -1.
 */
int kc_flow_dup_artifact_fd(int fd, char *error, size_t error_size) {
    int dup_fd;

    dup_fd = kc_flow_platform_dup_fd(fd);
    if (dup_fd < 0 || kc_flow_platform_rewind_fd(dup_fd) != 0) {
        snprintf(error, error_size, "Unable to duplicate artifact descriptor.");
        return -1;
    }
    return dup_fd;
}
