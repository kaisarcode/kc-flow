/**
 * graph.h
 * Summary: Flow graph validation and execution surface for composed flows.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#ifndef KC_FLOW_GRAPH_H
#define KC_FLOW_GRAPH_H

#include <stddef.h>

#include "model.h"

int kc_flow_run_flow(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *path,
    kc_flow_overrides *outputs,
    char *error,
    size_t error_size
);

#endif
