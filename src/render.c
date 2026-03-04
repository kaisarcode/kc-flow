#include "render.h"

#include <stdio.h>

int kc_flow_render_cli(
    const char *path,
    char *error,
    size_t error_size
) {
    (void)path;
    snprintf(error, error_size, "CLI rendering is not implemented yet.");
    return -1;
}
