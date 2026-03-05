/**
 * win.c
 * Summary: PowerShell renderer for contract and flow CLI export.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "win.h"

#include "link.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int kc_flow_find_node_index(char node_ids[][128], size_t count, const char *node_id) {
    size_t i;
    for (i = 0; i < count; ++i) {
        if (strcmp(node_ids[i], node_id) == 0) return (int)i;
    }
    return -1;
}

static int kc_flow_find_node_record(const kc_flow_model *model, const char *node_id) {
    size_t i;
    char key[128];
    for (i = 0; i < model->nodes.count; ++i) {
        const char *id;
        snprintf(key, sizeof(key), "node.%d.id", model->nodes.values[i]);
        id = kc_flow_model_get(model, key);
        if (id != NULL && strcmp(id, node_id) == 0) return model->nodes.values[i];
    }
    return -1;
}

static void kc_flow_sanitize(char *out, size_t out_size, const char *in) {
    size_t i, n = 0;
    if (out_size == 0) return;
    for (i = 0; in[i] != '\0' && n + 1 < out_size; ++i) {
        char c = in[i];
        out[n++] = (isalnum((unsigned char)c) || c == '_') ? c : '_';
    }
    if (n == 0) out[n++] = 'x';
    if (isdigit((unsigned char)out[0]) && n + 1 < out_size) {
        memmove(out + 1, out, n);
        out[0] = 'v';
        n++;
    }
    out[n] = '\0';
}

static void kc_flow_print_psq(const char *text) {
    size_t i;
    putchar('\'');
    for (i = 0; text[i] != '\0'; ++i) {
        if (text[i] == '\'') fputs("''", stdout);
        else putchar(text[i]);
    }
    putchar('\'');
}

static int kc_flow_append(char **buffer, size_t *capacity, size_t *length, const char *text) {
    size_t need = *length + strlen(text) + 1;
    char *grown;
    if (need > *capacity) {
        size_t next = *capacity;
        while (next < need) next *= 2;
        grown = realloc(*buffer, next);
        if (grown == NULL) return -1;
        *buffer = grown;
        *capacity = next;
    }
    memcpy(*buffer + *length, text, strlen(text));
    *length += strlen(text);
    (*buffer)[*length] = '\0';
    return 0;
}

static int kc_flow_build_template_to_ps(const char *input, char **out, char *error, size_t error_size) {
    size_t cap = strlen(input) + 64, len = 0, i;
    char *buf = malloc(cap);
    if (buf == NULL) {
        snprintf(error, error_size, "Out of memory while rendering template.");
        return -1;
    }
    buf[0] = '\0';
    for (i = 0; input[i] != '\0'; ++i) {
        if (input[i] == '<') {
            size_t start = i + 1, end = start;
            char ref[128], repl[140];
            const char *id = NULL;
            while (input[end] != '\0' && input[end] != '>') end++;
            if (input[end] != '>' || end <= start || (end - start) >= sizeof(ref)) {
                free(buf);
                snprintf(error, error_size, "Invalid placeholder in template.");
                return -1;
            }
            memcpy(ref, input + start, end - start);
            ref[end - start] = '\0';
            if (strncmp(ref, "input.", 6) == 0) id = ref + 6;
            else if (strncmp(ref, "param.", 6) == 0) id = ref + 6;
            else {
                free(buf);
                snprintf(error, error_size, "Unsupported placeholder: <%s>", ref);
                return -1;
            }
            snprintf(repl, sizeof(repl), "$%s", id);
            if (kc_flow_append(&buf, &cap, &len, repl) != 0) {
                free(buf);
                snprintf(error, error_size, "Out of memory while rendering template.");
                return -1;
            }
            i = end;
            continue;
        }
        {
            char one[2] = { input[i], '\0' };
            if (kc_flow_append(&buf, &cap, &len, one) != 0) {
                free(buf);
                snprintf(error, error_size, "Out of memory while rendering template.");
                return -1;
            }
        }
    }
    *out = buf;
    return 0;
}

static int kc_flow_toposort(char node_ids[][128], size_t node_count, const kc_flow_link_entry *links, size_t link_count, size_t *order, char *error, size_t error_size) {
    unsigned char edges[KC_STDIO_MAX_INDEXES][KC_STDIO_MAX_INDEXES];
    size_t indegree[KC_STDIO_MAX_INDEXES], queue[KC_STDIO_MAX_INDEXES];
    size_t i, head = 0, tail = 0, out_n = 0;
    memset(edges, 0, sizeof(edges));
    memset(indegree, 0, sizeof(indegree));
    for (i = 0; i < link_count; ++i) {
        if (links[i].from.kind == KC_FLOW_ENDPOINT_NODE_OUT && links[i].to.kind == KC_FLOW_ENDPOINT_NODE_IN) {
            int u = kc_flow_find_node_index(node_ids, node_count, links[i].from.node_id);
            int v = kc_flow_find_node_index(node_ids, node_count, links[i].to.node_id);
            if (u < 0 || v < 0) {
                snprintf(error, error_size, "Unable to build topological order.");
                return -1;
            }
            if (!edges[u][v]) {
                edges[u][v] = 1;
                indegree[v]++;
            }
        }
    }
    for (i = 0; i < node_count; ++i) if (indegree[i] == 0) queue[tail++] = i;
    while (head < tail) {
        size_t u = queue[head++], v;
        order[out_n++] = u;
        for (v = 0; v < node_count; ++v) if (edges[u][v]) {
            if (indegree[v] > 0) indegree[v]--;
            if (indegree[v] == 0) queue[tail++] = v;
        }
    }
    if (out_n != node_count) {
        snprintf(error, error_size, "Unable to topologically sort flow nodes.");
        return -1;
    }
    return 0;
}

int kc_flow_build_contract_ps(const kc_flow_model *model, const char *path, char *error, size_t error_size) {
    size_t i;
    char *script = NULL, *exec = NULL;
    char cwd[1024], workdir[1024];
    kc_flow_dirname(path, cwd, sizeof(cwd));
    if (kc_flow_build_path(workdir, sizeof(workdir), cwd, model->runtime_workdir != NULL ? model->runtime_workdir : ".") != 0) {
        snprintf(error, error_size, "Unable to resolve runtime.workdir.");
        return -1;
    }
    if (kc_flow_build_template_to_ps(model->runtime_script, &script, error, error_size) != 0) return -1;
    if (model->runtime_exec != NULL && model->runtime_exec[0] != '\0') {
        if (kc_flow_build_template_to_ps(model->runtime_exec, &exec, error, error_size) != 0) {
            free(script);
            return -1;
        }
    }

    printf("Set-StrictMode -Version Latest\n$ErrorActionPreference = 'Stop'\n");
    printf("# Contract: %s\n", model->id != NULL ? model->id : "unknown");
    for (i = 0; i < model->params.count; ++i) {
        char key[128];
        const char *id, *def;
        snprintf(key, sizeof(key), "param.%d.id", model->params.values[i]);
        id = kc_flow_model_get(model, key);
        if (id == NULL) continue;
        def = kc_flow_lookup_indexed_id_value(model, &model->params, "param.", id, "default");
        printf("if (-not $%s) { $%s = ", id, id);
        kc_flow_print_psq(def != NULL ? def : "");
        printf(" }\n");
    }
    for (i = 0; i < model->inputs.count; ++i) {
        char key[128];
        const char *id;
        snprintf(key, sizeof(key), "input.%d.id", model->inputs.values[i]);
        id = kc_flow_model_get(model, key);
        if (id != NULL) printf("if (-not $%s) { throw \"missing input '%s'\" }\n", id, id);
    }
    printf("Push-Location ");
    kc_flow_print_psq(workdir);
    printf("\ntry {\n");
    if (exec != NULL) printf("  & %s %s\n", exec, script);
    else printf("  %s\n", script);
    printf("} finally {\n  Pop-Location\n}\n");
    free(script);
    free(exec);
    return 0;
}

int kc_flow_build_flow_ps(const kc_flow_model *model, const char *path, char *error, size_t error_size) {
    kc_flow_link_entry links[KC_STDIO_MAX_INDEXES];
    char node_ids[KC_STDIO_MAX_INDEXES][128], flow_dir[1024];
    size_t order[KC_STDIO_MAX_INDEXES], node_count = 0, link_count = 0, i;
    kc_flow_dirname(path, flow_dir, sizeof(flow_dir));
    if (kc_flow_collect_node_ids(model, node_ids, &node_count, error, error_size) != 0) return -1;
    if (kc_flow_collect_links(model, node_ids, node_count, links, &link_count, error, error_size) != 0) return -1;
    if (kc_flow_detect_cycle(model, node_ids, node_count, error, error_size) != 0) return -1;
    if (kc_flow_toposort(node_ids, node_count, links, link_count, order, error, error_size) != 0) return -1;

    printf("Set-StrictMode -Version Latest\n$ErrorActionPreference = 'Stop'\n");
    printf("if (-not $KC_FLOW_BIN) { $KC_FLOW_BIN = 'kc-flow' }\n$V = @{}\n\n");
    for (i = 0; i < model->inputs.count; ++i) {
        char key[128];
        const char *id;
        snprintf(key, sizeof(key), "input.%d.id", model->inputs.values[i]);
        id = kc_flow_model_get(model, key);
        if (id != NULL) {
            printf("if (-not $%s) { throw \"missing input '%s'\" }\n", id, id);
            printf("$V['input.%s'] = $%s\n", id, id);
        }
    }
    printf("\n");

    for (i = 0; i < node_count; ++i) {
        size_t idx = order[i], j;
        const char *node_id = node_ids[idx], *contract_rel;
        int node_record = kc_flow_find_node_record(model, node_id);
        char key[128], node_var[160], contract_path[1024];
        if (node_record < 0) {
            snprintf(error, error_size, "Unable to resolve node in build flow.");
            return -1;
        }
        snprintf(key, sizeof(key), "node.%d.contract", node_record);
        contract_rel = kc_flow_model_get(model, key);
        if (contract_rel == NULL || kc_flow_build_path(contract_path, sizeof(contract_path), flow_dir, contract_rel) != 0) {
            snprintf(error, error_size, "Unable to resolve node path in build flow.");
            return -1;
        }
        kc_flow_sanitize(node_var, sizeof(node_var), node_id);
        printf("Write-Error \"[kc-flow] step node=%s contract=%s\"\n", node_id, contract_path);
        printf("$step_%s = & $KC_FLOW_BIN --run ", node_var);
        kc_flow_print_psq(contract_path);
        for (j = 0; j < link_count; ++j) if (links[j].to.kind == KC_FLOW_ENDPOINT_NODE_IN && strcmp(links[j].to.node_id, node_id) == 0) {
            char src_key[192];
            if (links[j].from.kind == KC_FLOW_ENDPOINT_INPUT) snprintf(src_key, sizeof(src_key), "input.%s", links[j].from.field_id);
            else snprintf(src_key, sizeof(src_key), "node.%s.out.%s", links[j].from.node_id, links[j].from.field_id);
            printf(" --set \"input.%s=$($V['%s'])\"", links[j].to.field_id, src_key);
        }
        printf("\nif ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }\n");
        for (j = 0; j < link_count; ++j) if (links[j].from.kind == KC_FLOW_ENDPOINT_NODE_OUT && strcmp(links[j].from.node_id, node_id) == 0) {
            size_t prefix_len = 7 + strlen(links[j].from.field_id);
            printf("$line = $step_%s | Where-Object { $_ -like 'output.%s=*' } | Select-Object -Last 1\n", node_var, links[j].from.field_id);
            printf("if (-not $line) { Write-Error \"missing output.%s from node %s\"; exit 1 }\n", links[j].from.field_id, node_id);
            printf("$V['node.%s.out.%s'] = $line.Substring(%zu)\n", node_id, links[j].from.field_id, prefix_len);
        }
        printf("\n");
    }
    for (i = 0; i < link_count; ++i) if (links[i].to.kind == KC_FLOW_ENDPOINT_OUTPUT) {
        if (links[i].from.kind == KC_FLOW_ENDPOINT_INPUT) printf("$V['output.%s'] = $V['input.%s']\n", links[i].to.field_id, links[i].from.field_id);
        else printf("$V['output.%s'] = $V['node.%s.out.%s']\n", links[i].to.field_id, links[i].from.node_id, links[i].from.field_id);
    }
    for (i = 0; i < model->outputs.count; ++i) {
        char key[128];
        const char *id;
        snprintf(key, sizeof(key), "output.%d.id", model->outputs.values[i]);
        id = kc_flow_model_get(model, key);
        if (id != NULL) printf("Write-Output (\"output.%s=\" + $V['output.%s'])\n", id, id);
    }
    return 0;
}
