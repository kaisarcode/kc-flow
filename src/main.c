/**
 * main.c
 * Summary: CLI entrypoint for headless flow execution.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"

#include <stdio.h>
#include <string.h>

/**
 * Prints command help.
 * @param bin Executable name.
 * @return void
 */
static void kc_flow_help(const char *bin) {
    printf("Options:\n");
    printf("  --run <file>      Execute one flow file\n");
    printf("  --set key=value   Inject one input or param override\n");
    printf("  --fd-in <n>       Reserved runtime input descriptor\n");
    printf("  --fd-out <n>      Reserved runtime output descriptor\n");
    printf("  --help            Show help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --run /path/to/file.flow\n", bin);
    printf("  %s --run /path/to/file.flow --set input.user_text=hello\n", bin);
}

/**
 * Fails one CLI invocation.
 * @param bin Executable name.
 * @param message Error message.
 * @return int Always 1.
 */
static int kc_flow_fail(const char *bin, const char *message) {
    fprintf(stderr, "Error: %s\n", message);
    kc_flow_help(bin);
    return 1;
}

/**
 * Program entrypoint.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return int 0 on success; non-zero on failure.
 */
int main(int argc, char **argv) {
    kc_flow_overrides overrides;
    const char *run_path;
    char error[256];
    int i;

    kc_flow_overrides_init(&overrides);
    run_path = NULL;
    if (argc <= 1) {
        kc_flow_help(argv[0]);
        return 0;
    }
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            kc_flow_help(argv[0]);
            kc_flow_overrides_free(&overrides);
            return 0;
        }
        if (strcmp(argv[i], "--run") == 0) {
            if (i + 1 >= argc) {
                kc_flow_overrides_free(&overrides);
                return kc_flow_fail(argv[0], "--run requires a file path.");
            }
            run_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--set") == 0) {
            const char *assignment;
            const char *equals;
            char key[256];
            size_t key_len;

            if (i + 1 >= argc) {
                kc_flow_overrides_free(&overrides);
                return kc_flow_fail(argv[0], "--set requires key=value.");
            }
            assignment = argv[++i];
            equals = strchr(assignment, '=');
            if (equals == NULL || equals == assignment) {
                kc_flow_overrides_free(&overrides);
                return kc_flow_fail(argv[0], "Invalid --set assignment.");
            }
            key_len = (size_t)(equals - assignment);
            if (key_len >= sizeof(key)) {
                kc_flow_overrides_free(&overrides);
                return kc_flow_fail(argv[0], "--set key too long.");
            }
            memcpy(key, assignment, key_len);
            key[key_len] = '\0';
            if (kc_flow_overrides_add(&overrides, key, equals + 1) != 0) {
                kc_flow_overrides_free(&overrides);
                return kc_flow_fail(argv[0], "Unable to register override.");
            }
            continue;
        }
        if (strcmp(argv[i], "--fd-in") == 0 || strcmp(argv[i], "--fd-out") == 0) {
            if (i + 1 >= argc) {
                kc_flow_overrides_free(&overrides);
                return kc_flow_fail(argv[0], "Descriptor flag requires a value.");
            }
            ++i;
            continue;
        }
        kc_flow_overrides_free(&overrides);
        return kc_flow_fail(argv[0], "Unknown argument.");
    }
    if (run_path == NULL) {
        kc_flow_overrides_free(&overrides);
        return kc_flow_fail(argv[0], "Missing --run.");
    }
    {
        kc_flow_model model;
        kc_flow_overrides outputs;
        int rc;

        kc_flow_model_init(&model);
        kc_flow_overrides_init(&outputs);
        if (!kc_flow_file_exists(run_path)) {
            kc_flow_overrides_free(&overrides);
            return kc_flow_fail(argv[0], "contract or flow file not found.");
        }
        if (kc_flow_load_file(run_path, &model, error, sizeof(error)) != 0 ||
                kc_flow_validate_model(&model, error, sizeof(error)) != 0) {
            fprintf(stderr, "Error: %s\n", error);
            kc_flow_model_free(&model);
            kc_flow_overrides_free(&outputs);
            kc_flow_overrides_free(&overrides);
            return 1;
        }
        if (model.kind == KC_FLOW_FILE_FLOW) {
            size_t j;

            rc = kc_flow_run_flow(&model, &overrides, run_path, &outputs, error, sizeof(error));
            if (rc == 0) {
                printf("run ok\n");
                printf("path=%s\n", run_path);
                printf("kind=flow\n");
                printf("id=%s\n", model.id);
                for (j = 0; j < outputs.count; ++j) {
                    printf("%s=%s\n", outputs.records[j].key, outputs.records[j].value);
                }
            }
        } else {
            kc_flow_run_output output;

            rc = kc_flow_run_contract(&model, &overrides, run_path, &output, error, sizeof(error));
            if (rc == 0 && output.exit_code == 0) {
                printf("run ok\n");
                printf("path=%s\n", run_path);
                printf("kind=contract\n");
                printf("id=%s\n", model.id);
                rc = kc_flow_print_contract_outputs(&model, &output);
            } else if (rc == 0) {
                snprintf(error, sizeof(error), "Contract exited with non-zero status.");
                rc = -1;
            }
            kc_flow_run_output_free(&output);
        }
        if (rc != 0) {
            fprintf(stderr, "Error: %s\n", error);
        }
        kc_flow_model_free(&model);
        kc_flow_overrides_free(&outputs);
        kc_flow_overrides_free(&overrides);
        return rc == 0 ? 0 : 1;
    }
}
