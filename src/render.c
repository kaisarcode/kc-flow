/**
 * render.c
 * Summary: CLI renderer/export backend entry points.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "render.h"

#include <stdio.h>

/**
 * Renders one flow into CLI form.
 * @param path Source flow path.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero while renderer is not implemented.
 */
int kc_flow_render_cli(
    const char *path,
    char *error,
    size_t error_size
) {
    (void)path;
    snprintf(error, error_size, "CLI rendering is not implemented yet.");
    return -1;
}
