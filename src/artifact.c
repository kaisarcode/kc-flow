/**
 * artifact.c
 * Summary: Descriptor-level artifact helpers for kc-flow runtime transport.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#define _POSIX_C_SOURCE 200809L

#include "flow.h"
#include "compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/**
 * Stores one artifact descriptor by endpoint.
 * @param artifacts Runtime artifact store.
 * @param key Endpoint key.
 * @param fd Descriptor.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_artifact_store(kc_flow_overrides *artifacts, const char *key, int fd) {
    char value[32];

    snprintf(value, sizeof(value), "%d", fd);
    return kc_flow_overrides_add(artifacts, key, value);
}

/**
 * Builds one refcount key for one endpoint.
 * @param endpoint Endpoint key.
 * @param key Output key buffer.
 * @param size Output key size.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_ref_key(const char *endpoint, char *key, size_t size) {
    return snprintf(key, size, "__ref__:%s", endpoint) >= (int)size ? -1 : 0;
}

/**
 * Loads one endpoint reference count.
 * @param artifacts Runtime artifact store.
 * @param endpoint Endpoint key.
 * @return int Reference count or 0.
 */
int kc_flow_artifact_refcount_load(const kc_flow_overrides *artifacts, const char *endpoint) {
    char key[192];
    const char *value;

    if (kc_flow_ref_key(endpoint, key, sizeof(key)) != 0) {
        return 0;
    }
    value = kc_flow_overrides_get(artifacts, key);
    return value != NULL ? atoi(value) : 0;
}

/**
 * Stores one endpoint reference count.
 * @param artifacts Runtime artifact store.
 * @param endpoint Endpoint key.
 * @param count Reference count.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_artifact_refcount_store(kc_flow_overrides *artifacts, const char *endpoint, int count) {
    char key[192];
    char value[32];

    if (kc_flow_ref_key(endpoint, key, sizeof(key)) != 0) {
        return -1;
    }
    snprintf(value, sizeof(value), "%d", count);
    return kc_flow_overrides_add(artifacts, key, value);
}

/**
 * Loads one stored artifact descriptor.
 * @param artifacts Runtime artifact store.
 * @param key Endpoint key.
 * @return int Descriptor or -1.
 */
int kc_flow_artifact_load(const kc_flow_overrides *artifacts, const char *key) {
    const char *value;

    value = kc_flow_overrides_get(artifacts, key);
    if (value == NULL || value[0] == '\0') {
        return -1;
    }
    return atoi(value);
}

/**
 * Duplicates one stored artifact and releases one consumer reference.
 * @param artifacts Runtime artifact store.
 * @param endpoint Endpoint key.
 * @param fd Output duplicated descriptor.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_artifact_consume(kc_flow_overrides *artifacts, const char *endpoint, int *fd, char *error, size_t error_size) {
    int source_fd;
    int refcount;

    source_fd = kc_flow_artifact_load(artifacts, endpoint);
    if (source_fd < 0) {
        snprintf(error, error_size, "Missing runtime artifact.");
        return -1;
    }
    *fd = kc_flow_dup_artifact_fd(source_fd, error, error_size);
    if (*fd < 0) {
        return -1;
    }
    refcount = kc_flow_artifact_refcount_load(artifacts, endpoint);
    if (refcount > 0 && kc_flow_artifact_refcount_store(artifacts, endpoint, refcount - 1) != 0) {
        kc_flow_release_fd(*fd);
        snprintf(error, error_size, "Unable to update artifact refcount.");
        return -1;
    }
    if (refcount == 1) {
        kc_flow_release_fd(source_fd);
        if (kc_flow_artifact_store(artifacts, endpoint, -1) != 0) {
            kc_flow_release_fd(*fd);
            snprintf(error, error_size, "Unable to retire runtime artifact.");
            return -1;
        }
    }
    return 0;
}

/**
 * Closes every stored artifact descriptor.
 * @param artifacts Runtime artifact store.
 * @return void
 */
void kc_flow_cleanup_artifacts(kc_flow_overrides *artifacts) {
    size_t i;

    for (i = 0; i < artifacts->count; ++i) {
        int fd;

        if (strncmp(artifacts->records[i].key, "__ref__:", 8) == 0) {
            continue;
        }
        fd = atoi(artifacts->records[i].value);
        if (fd >= 0) {
            kc_flow_release_fd(fd);
        }
    }
}
