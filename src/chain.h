/**
 * chain.h
 * Summary: Flow CLI chain builder API.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#ifndef KC_FLOW_CHAIN_H
#define KC_FLOW_CHAIN_H

#include <stddef.h>

#include "model.h"

int kc_flow_build_flow_cli(const kc_flow_model *model,
                           const char *path,
                           char *error,
                           size_t error_size);

#endif
