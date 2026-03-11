/**
 * load_util.h
 * Summary: Internal parsing helpers for flow and contract file loading.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#ifndef KC_FLOW_LOAD_UTIL_H
#define KC_FLOW_LOAD_UTIL_H

#include <stdio.h>
#include <sys/types.h>

#include "model.h"

char *kc_flow_trim(char *text);
char *kc_flow_load_strdup(const char *text);
int kc_flow_model_collect(kc_flow_model *model);
ssize_t kc_flow_getline_portable(char **lineptr, size_t *n, FILE *stream);

#endif
