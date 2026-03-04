#define _POSIX_C_SOURCE 200809L

#include "runtime.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <sys/wait.h>
#endif
#include <unistd.h>

static int kc_flow_system_exit_code(int status) {
#if defined(_WIN32)
    return status;
#else
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}

void kc_flow_run_output_free(kc_flow_run_output *output) {
    free(output->stdout_text);
    free(output->stderr_text);
    output->stdout_text = NULL;
    output->stderr_text = NULL;
    output->exit_code = 0;
}

static char *kc_flow_strdup(const char *text) {
    size_t len;
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    len = strlen(text);
    copy = malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, len + 1);
    return copy;
}

static const char *kc_flow_resolve_reference_raw(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *reference
) {
    const char *field_id;
    const char *override_value;

    if (overrides != NULL) {
        override_value = kc_flow_overrides_get(overrides, reference);
        if (override_value != NULL) {
            return override_value;
        }
    }

    if (strncmp(reference, "param.", 6) == 0) {
        field_id = reference + 6;
        return kc_flow_lookup_indexed_id_value(model, &model->params, "param.", field_id, "default");
    }

    if (strncmp(reference, "input.", 6) == 0) {
        return NULL;
    }

    if (strncmp(reference, "output.", 7) == 0) {
        return NULL;
    }

    return NULL;
}

static char *kc_flow_resolve_template(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *template_text,
    char *error,
    size_t error_size
) {
    size_t capacity;
    size_t length;
    size_t i;
    char *result;

    if (template_text == NULL) {
        return NULL;
    }

    capacity = strlen(template_text) + 1;
    result = malloc(capacity);
    if (result == NULL) {
        snprintf(error, error_size, "Out of memory while resolving template.");
        return NULL;
    }

    length = 0;
    for (i = 0; template_text[i] != '\0'; ++i) {
        if (template_text[i] == '<') {
            size_t start;
            size_t end;
            char reference[256];
            size_t ref_len;
            const char *value;
            size_t value_len;

            start = i + 1;
            end = start;
            while (template_text[end] != '\0' && template_text[end] != '>') {
                end++;
            }

            if (template_text[end] != '>') {
                free(result);
                snprintf(error, error_size, "Unclosed placeholder in template.");
                return NULL;
            }

            ref_len = end - start;
            if (ref_len == 0 || ref_len >= sizeof(reference)) {
                free(result);
                snprintf(error, error_size, "Invalid placeholder length in template.");
                return NULL;
            }

            memcpy(reference, template_text + start, ref_len);
            reference[ref_len] = '\0';

            value = kc_flow_resolve_reference_raw(model, overrides, reference);
            if (value == NULL) {
                free(result);
                snprintf(error, error_size, "Unable to resolve placeholder: <%.180s>", reference);
                return NULL;
            }

            value_len = strlen(value);
            while (length + value_len + 1 > capacity) {
                char *grown;

                capacity *= 2;
                grown = realloc(result, capacity);
                if (grown == NULL) {
                    free(result);
                    snprintf(error, error_size, "Out of memory while growing template.");
                    return NULL;
                }
                result = grown;
            }

            memcpy(result + length, value, value_len);
            length += value_len;
            i = end;
            continue;
        }

        if (length + 2 > capacity) {
            char *grown;

            capacity *= 2;
            grown = realloc(result, capacity);
            if (grown == NULL) {
                free(result);
                snprintf(error, error_size, "Out of memory while growing template.");
                return NULL;
            }
            result = grown;
        }

        result[length++] = template_text[i];
    }

    result[length] = '\0';
    return result;
}

static char *kc_flow_shell_quote(const char *text) {
    size_t i;
    size_t extra;
    char *quoted;
    size_t length;

    if (text == NULL) {
        return kc_flow_strdup("''");
    }

    extra = 2;
    for (i = 0; text[i] != '\0'; ++i) {
        extra += text[i] == '\'' ? 4 : 1;
    }

    quoted = malloc(extra + 1);
    if (quoted == NULL) {
        return NULL;
    }

    length = 0;
    quoted[length++] = '\'';
    for (i = 0; text[i] != '\0'; ++i) {
        if (text[i] == '\'') {
            memcpy(quoted + length, "'\\''", 4);
            length += 4;
        } else {
            quoted[length++] = text[i];
        }
    }
    quoted[length++] = '\'';
    quoted[length] = '\0';

    return quoted;
}

static char *kc_flow_read_file_text(const char *path) {
    FILE *fp;
    long size;
    char *buffer;
    size_t read_size;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }

    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    buffer = malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(fp);
        return NULL;
    }

    read_size = fread(buffer, 1, (size_t)size, fp);
    buffer[read_size] = '\0';
    fclose(fp);
    return buffer;
}

static int kc_flow_write_temp_file(const char *content, char *path_buffer, size_t path_size) {
    int fd;
    FILE *fp;
    char template_path[] = "/tmp/kc-flow-stdin-XXXXXX";

    fd = mkstemp(template_path);
    if (fd < 0) {
        return -1;
    }

    fp = fdopen(fd, "w");
    if (fp == NULL) {
        close(fd);
        unlink(template_path);
        return -1;
    }

    if (content != NULL && fputs(content, fp) == EOF) {
        fclose(fp);
        unlink(template_path);
        return -1;
    }

    if (fclose(fp) != 0) {
        unlink(template_path);
        return -1;
    }

    if (snprintf(path_buffer, path_size, "%s", template_path) >= (int)path_size) {
        unlink(template_path);
        return -1;
    }

    return 0;
}

static int kc_flow_find_output_binding(const kc_flow_model *model, const char *output_id, const char **mode, const char **path) {
    size_t i;
    char key[128];
    const char *current_id;

    for (i = 0; i < model->bind_output.count; ++i) {
        snprintf(key, sizeof(key), "bind.output.%d.id", model->bind_output.values[i]);
        current_id = kc_flow_model_get(model, key);
        if (current_id != NULL && strcmp(current_id, output_id) == 0) {
            snprintf(key, sizeof(key), "bind.output.%d.mode", model->bind_output.values[i]);
            *mode = kc_flow_model_get(model, key);
            snprintf(key, sizeof(key), "bind.output.%d.path", model->bind_output.values[i]);
            *path = kc_flow_model_get(model, key);
            return 0;
        }
    }

    return -1;
}

static int kc_flow_is_valid_env_key(const char *key) {
    size_t i;

    if (key == NULL || key[0] == '\0') {
        return 0;
    }

    if (!(isalpha((unsigned char)key[0]) || key[0] == '_')) {
        return 0;
    }

    for (i = 1; key[i] != '\0'; ++i) {
        if (!(isalnum((unsigned char)key[i]) || key[i] == '_')) {
            return 0;
        }
    }

    return 1;
}

static int kc_flow_build_env_prefix(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    char **out_prefix,
    char *error,
    size_t error_size
) {
    size_t i;
    char *buffer;
    size_t length;
    size_t capacity;

    buffer = calloc(1, 1);
    if (buffer == NULL) {
        snprintf(error, error_size, "Out of memory while building env prefix.");
        return -1;
    }

    length = 0;
    capacity = 1;

    for (i = 0; i < model->runtime_env.count; ++i) {
        char key_name[128];
        char value_name[128];
        const char *env_key;
        const char *env_value_template;
        char *env_value;
        char *quoted_env_value;
        size_t needed;
        int written;

        snprintf(key_name, sizeof(key_name), "runtime.env.%d.key", model->runtime_env.values[i]);
        snprintf(value_name, sizeof(value_name), "runtime.env.%d.value", model->runtime_env.values[i]);

        env_key = kc_flow_model_get(model, key_name);
        env_value_template = kc_flow_model_get(model, value_name);

        if (env_key == NULL || env_value_template == NULL) {
            continue;
        }

        if (!kc_flow_is_valid_env_key(env_key)) {
            free(buffer);
            snprintf(error, error_size, "Invalid env key: %s", env_key);
            return -1;
        }

        env_value = kc_flow_resolve_template(model, overrides, env_value_template, error, error_size);
        if (env_value == NULL) {
            free(buffer);
            return -1;
        }

        quoted_env_value = kc_flow_shell_quote(env_value);
        free(env_value);
        if (quoted_env_value == NULL) {
            free(buffer);
            snprintf(error, error_size, "Out of memory while quoting env value.");
            return -1;
        }

        needed = length + strlen(env_key) + strlen(quoted_env_value) + 3;
        if (needed + 1 > capacity) {
            char *grown;
            size_t new_capacity = capacity * 2;
            while (new_capacity < needed + 1) {
                new_capacity *= 2;
            }
            grown = realloc(buffer, new_capacity);
            if (grown == NULL) {
                free(quoted_env_value);
                free(buffer);
                snprintf(error, error_size, "Out of memory while growing env prefix.");
                return -1;
            }
            buffer = grown;
            capacity = new_capacity;
        }

        written = snprintf(buffer + length, capacity - length, "%s=%s ", env_key, quoted_env_value);
        free(quoted_env_value);
        if (written < 0 || (size_t)written >= capacity - length) {
            free(buffer);
            snprintf(error, error_size, "Unable to compose env prefix.");
            return -1;
        }
        length += (size_t)written;
    }

    *out_prefix = buffer;
    return 0;
}

int kc_flow_run_contract(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *cfg_path,
    kc_flow_run_output *output,
    char *error,
    size_t error_size
) {
    char cfg_dir[PATH_MAX];
    char workdir[PATH_MAX];
    char stdout_path[] = "/tmp/kc-flow-stdout-XXXXXX";
    char stderr_path[] = "/tmp/kc-flow-stderr-XXXXXX";
    char stdin_path[PATH_MAX];
    int stdout_fd;
    int stderr_fd;
    char *resolved_script;
    char *resolved_exec;
    char *resolved_stdin;
    char *quoted_workdir;
    char *quoted_stdout;
    char *quoted_stderr;
    char *quoted_stdin;
    char *quoted_exec;
    char *command;
    char *env_buffer;
    size_t command_size;
    int status;

    kc_flow_dirname(cfg_path, cfg_dir, sizeof(cfg_dir));
    if (kc_flow_build_path(workdir, sizeof(workdir), cfg_dir, model->runtime_workdir != NULL ? model->runtime_workdir : ".") != 0) {
        snprintf(error, error_size, "Unable to resolve workdir.");
        return -1;
    }

    resolved_script = kc_flow_resolve_template(model, overrides, model->runtime_script, error, error_size);
    if (resolved_script == NULL) {
        return -1;
    }

    resolved_exec = kc_flow_resolve_template(model, overrides, model->runtime_exec, error, error_size);
    if (model->runtime_exec != NULL && resolved_exec == NULL) {
        free(resolved_script);
        return -1;
    }

    resolved_stdin = kc_flow_resolve_template(model, overrides, model->runtime_stdin, error, error_size);
    if (model->runtime_stdin != NULL && resolved_stdin == NULL) {
        free(resolved_script);
        free(resolved_exec);
        return -1;
    }

    stdout_fd = mkstemp(stdout_path);
    stderr_fd = mkstemp(stderr_path);
    if (stdout_fd < 0 || stderr_fd < 0) {
        free(resolved_script);
        free(resolved_exec);
        free(resolved_stdin);
        if (stdout_fd >= 0) {
            close(stdout_fd);
            unlink(stdout_path);
        }
        if (stderr_fd >= 0) {
            close(stderr_fd);
            unlink(stderr_path);
        }
        snprintf(error, error_size, "Unable to create temporary output files.");
        return -1;
    }
    close(stdout_fd);
    close(stderr_fd);

    stdin_path[0] = '\0';
    if (resolved_stdin != NULL) {
        if (kc_flow_write_temp_file(resolved_stdin, stdin_path, sizeof(stdin_path)) != 0) {
            free(resolved_script);
            free(resolved_exec);
            free(resolved_stdin);
            unlink(stdout_path);
            unlink(stderr_path);
            snprintf(error, error_size, "Unable to create stdin file.");
            return -1;
        }
    }

    quoted_workdir = kc_flow_shell_quote(workdir);
    quoted_stdout = kc_flow_shell_quote(stdout_path);
    quoted_stderr = kc_flow_shell_quote(stderr_path);
    quoted_stdin = resolved_stdin != NULL ? kc_flow_shell_quote(stdin_path) : NULL;
    quoted_exec = resolved_exec != NULL ? kc_flow_shell_quote(resolved_exec) : NULL;
    env_buffer = NULL;

    if (quoted_workdir == NULL || quoted_stdout == NULL || quoted_stderr == NULL ||
        (resolved_stdin != NULL && quoted_stdin == NULL) ||
        (resolved_exec != NULL && quoted_exec == NULL)) {
        free(resolved_script);
        free(resolved_exec);
        free(resolved_stdin);
        free(quoted_workdir);
        free(quoted_stdout);
        free(quoted_stderr);
        free(quoted_stdin);
        free(quoted_exec);
        free(env_buffer);
        unlink(stdout_path);
        unlink(stderr_path);
        if (stdin_path[0] != '\0') {
            unlink(stdin_path);
        }
        snprintf(error, error_size, "Out of memory while preparing execution.");
        return -1;
    }

    if (kc_flow_build_env_prefix(model, overrides, &env_buffer, error, error_size) != 0) {
        free(resolved_script);
        free(resolved_exec);
        free(resolved_stdin);
        free(quoted_workdir);
        free(quoted_stdout);
        free(quoted_stderr);
        free(quoted_stdin);
        free(quoted_exec);
        if (stdin_path[0] != '\0') {
            unlink(stdin_path);
        }
        unlink(stdout_path);
        unlink(stderr_path);
        return -1;
    }

    command_size = strlen("cd ") + strlen(quoted_workdir) + strlen(" && ") +
                   strlen(env_buffer) + strlen(resolved_script) +
                   strlen(" > ") + strlen(quoted_stdout) +
                   strlen(" 2> ") + strlen(quoted_stderr) + 32;

    if (quoted_exec != NULL) {
        command_size += strlen(quoted_exec) + 1;
    }

    if (quoted_stdin != NULL) {
        command_size += strlen(" < ") + strlen(quoted_stdin);
    }

    command = malloc(command_size);
    if (command == NULL) {
        free(resolved_script);
        free(resolved_exec);
        free(resolved_stdin);
        free(quoted_workdir);
        free(quoted_stdout);
        free(quoted_stderr);
        free(quoted_stdin);
        free(quoted_exec);
        free(env_buffer);
        unlink(stdout_path);
        unlink(stderr_path);
        if (stdin_path[0] != '\0') {
            unlink(stdin_path);
        }
        snprintf(error, error_size, "Out of memory while building command.");
        return -1;
    }

    snprintf(
        command,
        command_size,
        "cd %s && %s%s%s%s%s > %s 2> %s",
        quoted_workdir,
        env_buffer,
        quoted_exec != NULL ? quoted_exec : "",
        quoted_exec != NULL ? " " : "",
        resolved_script,
        quoted_stdin != NULL ? "" : "",
        quoted_stdout,
        quoted_stderr
    );

    if (quoted_stdin != NULL) {
        size_t current_len = strlen(command);
        snprintf(command + current_len, command_size - current_len, " < %s", quoted_stdin);
    }

    status = system(command);
    if (status == -1) {
        snprintf(error, error_size, "Unable to launch process.");
        free(command);
        free(resolved_script);
        free(resolved_exec);
        free(resolved_stdin);
        free(quoted_workdir);
        free(quoted_stdout);
        free(quoted_stderr);
        free(quoted_stdin);
        free(quoted_exec);
        free(env_buffer);
        unlink(stdout_path);
        unlink(stderr_path);
        if (stdin_path[0] != '\0') {
            unlink(stdin_path);
        }
        return -1;
    }

    output->stdout_text = kc_flow_read_file_text(stdout_path);
    output->stderr_text = kc_flow_read_file_text(stderr_path);
    output->exit_code = kc_flow_system_exit_code(status);

    unlink(stdout_path);
    unlink(stderr_path);
    if (stdin_path[0] != '\0') {
        unlink(stdin_path);
    }

    free(command);
    free(resolved_script);
    free(resolved_exec);
    free(resolved_stdin);
    free(quoted_workdir);
    free(quoted_stdout);
    free(quoted_stderr);
    free(quoted_stdin);
    free(quoted_exec);
    free(env_buffer);

    if (output->stdout_text == NULL || output->stderr_text == NULL) {
        snprintf(error, error_size, "Unable to collect process output.");
        return -1;
    }

    return 0;
}

int kc_flow_print_contract_outputs(const kc_flow_model *model, const kc_flow_run_output *output) {
    size_t i;
    char key[128];

    for (i = 0; i < model->outputs.count; ++i) {
        const char *output_id;
        const char *mode;
        const char *path;

        snprintf(key, sizeof(key), "output.%d.id", model->outputs.values[i]);
        output_id = kc_flow_model_get(model, key);
        if (output_id == NULL) {
            continue;
        }

        mode = NULL;
        path = NULL;
        if (kc_flow_find_output_binding(model, output_id, &mode, &path) != 0 || mode == NULL) {
            continue;
        }

        if (strcmp(mode, "stdout") == 0) {
            printf("output.%s=%s\n", output_id, output->stdout_text);
        } else if (strcmp(mode, "stderr") == 0) {
            printf("output.%s=%s\n", output_id, output->stderr_text);
        } else if (strcmp(mode, "exit_code") == 0) {
            printf("output.%s=%d\n", output_id, output->exit_code);
        } else if (strcmp(mode, "file") == 0 && path != NULL) {
            printf("output.%s=%s\n", output_id, path);
        }
    }

    return 0;
}
