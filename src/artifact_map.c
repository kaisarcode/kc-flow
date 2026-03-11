/**
 * artifact_map.c
 * Summary: Runtime endpoint-to-artifact bookkeeping and refcount helpers.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"

#include <stdio.h>
#include <stdlib.h>

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
