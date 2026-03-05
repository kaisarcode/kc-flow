/**
 * shell.c
 * Summary: Shell and process helper utilities for runtime execution.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#define _POSIX_C_SOURCE 200809L

#include "priv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <sys/wait.h>
#endif
#include <unistd.h>

int kc_flow_system_exit_code(int status) {
#if defined(_WIN32)
    return status;
#else
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}

static char *kc_flow_strdup(const char *text) {
    size_t len;
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    len = strlen(text);
    copy = malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, len + 1);
    return copy;
}

char *kc_flow_shell_quote(const char *text) {
    size_t i;
    size_t extra;
    char *quoted;
    size_t length;

    if (text == NULL) {
        return kc_flow_strdup("''");
    }

    extra = 2;
    for (i = 0; text[i] != '\0'; ++i) {
        extra += text[i] == '\'' ? 4 : 1;
    }

    quoted = malloc(extra + 1);
    if (quoted == NULL) {
        return NULL;
    }

    length = 0;
    quoted[length++] = '\'';
    for (i = 0; text[i] != '\0'; ++i) {
        if (text[i] == '\'') {
            memcpy(quoted + length, "'\\''", 4);
            length += 4;
        } else {
            quoted[length++] = text[i];
        }
    }
    quoted[length++] = '\'';
    quoted[length] = '\0';

    return quoted;
}

char *kc_flow_read_file_text(const char *path) {
    FILE *fp;
    long size;
    char *buffer;
    size_t read_size;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }

    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    buffer = malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(fp);
        return NULL;
    }

    read_size = fread(buffer, 1, (size_t)size, fp);
    buffer[read_size] = '\0';
    fclose(fp);
    return buffer;
}

int kc_flow_write_temp_file(const char *content,
                            char *path_buffer,
                            size_t path_size) {
    int fd;
    FILE *fp;
    char template_path[] = "/tmp/kc-flow-stdin-XXXXXX";

    fd = mkstemp(template_path);
    if (fd < 0) {
        return -1;
    }

    fp = fdopen(fd, "w");
    if (fp == NULL) {
        close(fd);
        unlink(template_path);
        return -1;
    }

    if (content != NULL && fputs(content, fp) == EOF) {
        fclose(fp);
        unlink(template_path);
        return -1;
    }

    if (fclose(fp) != 0) {
        unlink(template_path);
        return -1;
    }

    if (snprintf(path_buffer, path_size, "%s", template_path) >= (int)path_size) {
        unlink(template_path);
        return -1;
    }

    return 0;
}
