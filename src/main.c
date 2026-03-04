/**
 * kc-flow
 * Summary: Headless parser and execution entry for contract and flow files.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include <stdio.h>
#include <string.h>

#include "graph.h"
#include "model.h"
#include "render.h"
#include "runtime.h"

static void kc_flow_help(const char *bin) {
    printf("Options:\n");
    printf("  --run <file>      Execute one flow file\n");
    printf("  --cli <file>      Render flow as terminal script\n");
    printf("  --help            Show help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --run /path/to/file.flow\n", bin);
    printf("  %s --run /path/to/file.flow --set input.user_text=hello\n", bin);
    printf("  %s --cli /path/to/file.flow\n", bin);
}

static int kc_flow_fail(const char *bin, const char *message) {
    fprintf(stderr, "Error: %s\n", message);
    kc_flow_help(bin);
    return 1;
}

static int kc_flow_parse_run_overrides(
    int argc,
    char **argv,
    int start_index,
    kc_flow_overrides *overrides,
    char *error,
    size_t error_size
) {
    int i;

    for (i = start_index; i < argc; ++i) {
        const char *assignment;
        const char *equals;
        char key[256];
        size_t key_len;

        if (strcmp(argv[i], "--set") != 0) {
            snprintf(error, error_size, "Unknown run argument: %s", argv[i]);
            return -1;
        }

        if (i + 1 >= argc) {
            snprintf(error, error_size, "run --set requires key=value.");
            return -1;
        }

        assignment = argv[++i];
        equals = strchr(assignment, '=');
        if (equals == NULL || equals == assignment) {
            snprintf(error, error_size, "Invalid --set assignment: %s", assignment);
            return -1;
        }

        key_len = (size_t)(equals - assignment);
        if (key_len >= sizeof(key)) {
            snprintf(error, error_size, "run --set key too long.");
            return -1;
        }

        memcpy(key, assignment, key_len);
        key[key_len] = '\0';

        if (kc_flow_overrides_add(overrides, key, equals + 1) != 0) {
            snprintf(error, error_size, "Unable to register run override.");
            return -1;
        }
    }

    return 0;
}

static int kc_flow_run(const char *path, const kc_flow_overrides *overrides) {
    kc_flow_model model;
    kc_flow_run_output output;
    char error[256];

    if (!kc_flow_file_exists(path)) {
        fprintf(stderr, "Error: contract or flow file not found: %s\n", path);
        return 1;
    }

    kc_flow_model_init(&model);
    memset(&output, 0, sizeof(output));

    if (kc_flow_load_file(path, &model, error, sizeof(error)) != 0 ||
        kc_flow_validate_model(&model, error, sizeof(error)) != 0) {
        fprintf(stderr, "Error: %s\n", error);
        kc_flow_model_free(&model);
        return 1;
    }

    if (model.kind == KC_STDIO_FILE_FLOW) {
        if (kc_flow_run_flow(&model, overrides, path, error, sizeof(error)) != 0) {
            fprintf(stderr, "Error: %s\n", error);
            kc_flow_model_free(&model);
            return 1;
        }
        kc_flow_model_free(&model);
        return 0;
    }

    if (kc_flow_run_contract(&model, overrides, path, &output, error, sizeof(error)) != 0) {
        fprintf(stderr, "Error: %s\n", error);
        kc_flow_run_output_free(&output);
        kc_flow_model_free(&model);
        return 1;
    }

    printf("run ok\n");
    printf("path=%s\n", path);
    printf("kind=contract\n");
    printf("id=%s\n", model.id);
    printf("engine=headless\n");
    printf("exit_code=%d\n", output.exit_code);
    kc_flow_print_contract_outputs(&model, &output);

    kc_flow_run_output_free(&output);
    kc_flow_model_free(&model);
    return output.exit_code == 0 ? 0 : 1;
}

static int kc_flow_cli(const char *path) {
    char error[256];

    if (!kc_flow_file_exists(path)) {
        fprintf(stderr, "Error: contract or flow file not found: %s\n", path);
        return 1;
    }

    if (kc_flow_render_cli(path, error, sizeof(error)) != 0) {
        fprintf(stderr, "Error: %s\n", error);
        return 1;
    }

    return 0;
}

int main(int argc, char **argv) {
    kc_flow_overrides overrides;
    char run_error[256];

    kc_flow_overrides_init(&overrides);

    if (argc <= 1) {
        kc_flow_help(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "--help") == 0) {
        kc_flow_help(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "--run") == 0) {
        if (argc < 3) {
            return kc_flow_fail(argv[0], "--run requires one file path.");
        }
        if (kc_flow_parse_run_overrides(argc, argv, 3, &overrides, run_error, sizeof(run_error)) != 0) {
            kc_flow_overrides_free(&overrides);
            return kc_flow_fail(argv[0], run_error);
        }
        {
            int rc = kc_flow_run(argv[2], &overrides);
            kc_flow_overrides_free(&overrides);
            return rc;
        }
    }

    if (strcmp(argv[1], "--cli") == 0) {
        if (argc != 3) {
            return kc_flow_fail(argv[0], "--cli requires one file path.");
        }
        return kc_flow_cli(argv[2]);
    }

    return kc_flow_fail(argv[0], "Unknown argument.");
}
