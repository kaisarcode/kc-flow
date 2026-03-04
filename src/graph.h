#ifndef KC_FLOW_GRAPH_H
#define KC_FLOW_GRAPH_H

#include <stddef.h>

#include "model.h"

int kc_flow_run_flow(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *path,
    char *error,
    size_t error_size
);

#endif
