/**
 * priv.h
 * Summary: Internal runtime helper declarations.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#ifndef KC_FLOW_PRIV_H
#define KC_FLOW_PRIV_H

#include <stddef.h>

#include "runtime.h"

int kc_flow_system_exit_code(int status);
char *kc_flow_shell_quote(const char *text);
char *kc_flow_read_file_text(const char *path);
int kc_flow_write_temp_file(const char *content,
                            char *path_buffer,
                            size_t path_size);

char *kc_flow_resolve_template(const kc_flow_model *model,
                               const kc_flow_overrides *overrides,
                               const char *template_text,
                               char *error,
                               size_t error_size);

int kc_flow_build_env_prefix(const kc_flow_model *model,
                             const kc_flow_overrides *overrides,
                             char **out_prefix,
                             char *error,
                             size_t error_size);

#endif
