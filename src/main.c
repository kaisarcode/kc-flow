/**
 * kc-stdio - Headless Contract Engine Scaffold
 * Summary: Minimal CLI surface for contract and flow execution direction.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define KC_STDIO_VERSION "0.1.0"

static void kc_stdio_usage(const char *bin) {
    printf("Commands:\n");
    printf("  schema            Print the current contract and flow direction\n");
    printf("  inspect <file>    Inspect one contract or flow file path\n");
    printf("  run <file>        Resolve one contract or flow file path for execution\n");
    printf("  -v, --version     Show version\n");
    printf("  -h, --help        Show help\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s schema\n", bin);
    printf("  %s inspect path/to/file.kcs\n", bin);
    printf("  %s run path/to/file.kcs\n", bin);
}

static int kc_stdio_fail(const char *bin, const char *message) {
    fprintf(stderr, "Error: %s\n", message);
    kc_stdio_usage(bin);
    return 1;
}

static int kc_stdio_file_exists(const char *path) {
    struct stat st;
    return path != NULL && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int kc_stdio_schema(void) {
    printf("contract: executable unit with params, inputs, outputs, runtime, bindings\n");
    printf("flow: composed contract graph with nodes, links, and exposed interface\n");
    printf("engine: headless resolver and executor over contracts and flows\n");
    printf("gui: optional view and editor over the same model\n");
    return 0;
}

static int kc_stdio_inspect(const char *path) {
    if (!kc_stdio_file_exists(path)) {
        fprintf(stderr, "Error: contract or flow file not found: %s\n", path);
        return 1;
    }
    printf("inspect ok\n");
    printf("path=%s\n", path);
    return 0;
}

static int kc_stdio_run(const char *path) {
    if (!kc_stdio_file_exists(path)) {
        fprintf(stderr, "Error: contract or flow file not found: %s\n", path);
        return 1;
    }
    printf("run queued\n");
    printf("path=%s\n", path);
    printf("engine=headless\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc <= 1) {
        kc_stdio_usage(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        kc_stdio_usage(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
        printf("kc-stdio %s\n", KC_STDIO_VERSION);
        return 0;
    }

    if (strcmp(argv[1], "schema") == 0) {
        if (argc != 2) {
            return kc_stdio_fail(argv[0], "schema does not accept extra arguments.");
        }
        return kc_stdio_schema();
    }

    if (strcmp(argv[1], "inspect") == 0) {
        if (argc != 3) {
            return kc_stdio_fail(argv[0], "inspect requires exactly one file path.");
        }
        return kc_stdio_inspect(argv[2]);
    }

    if (strcmp(argv[1], "run") == 0) {
        if (argc != 3) {
            return kc_stdio_fail(argv[0], "run requires exactly one file path.");
        }
        return kc_stdio_run(argv[2]);
    }

    return kc_stdio_fail(argv[0], "Unknown argument.");
}
