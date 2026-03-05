/**
 * win.h
 * Summary: PowerShell chain renderer helpers.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#ifndef KC_FLOW_WIN_H
#define KC_FLOW_WIN_H

#include <stddef.h>

#include "model.h"

int kc_flow_build_flow_ps(const kc_flow_model *model,
                          const char *path,
                          char *error,
                          size_t error_size);
int kc_flow_build_contract_ps(const kc_flow_model *model,
                              const char *path,
                              char *error,
                              size_t error_size);

#endif
