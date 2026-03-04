#include "graph.h"

#include <stdio.h>

int kc_flow_run_flow(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *path,
    char *error,
    size_t error_size
) {
    (void)model;
    (void)overrides;
    (void)path;
    snprintf(error, error_size, "Flow execution is not implemented yet.");
    return -1;
}
