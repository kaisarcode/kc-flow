/**
 * validate.c
 * Summary: Template resolution, scope expansion, and structural validation.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Resolves one placeholder lookup.
 * @param name Placeholder name.
 * @param flow_params Current flow scope.
 * @param node_params Current node scope.
 * @return const char* Resolved value or NULL.
 */
static const char *kc_flow_resolve_scope_value(
    const char *name,
    const kc_flow_overrides *flow_params,
    const kc_flow_overrides *node_params
) {
    if (strncmp(name, "flow.param.", 11) == 0) {
        return kc_flow_overrides_get(flow_params, name + 11);
    }
    if (strncmp(name, "node.param.", 11) == 0) {
        return kc_flow_overrides_get(node_params, name + 11);
    }
    return NULL;
}

/**
 * Resolves one template string in flow/node scope.
 * @param text Source template.
 * @param flow_params Current flow scope.
 * @param node_params Current node scope.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return char* Resolved string or NULL.
 */
char *kc_flow_resolve_template(
    const char *text,
    const kc_flow_overrides *flow_params,
    const kc_flow_overrides *node_params,
    char *error,
    size_t error_size
) {
    size_t i;
    size_t capacity;
    size_t length;
    char *result;

    if (text == NULL) {
        return NULL;
    }
    capacity = strlen(text) + 1;
    result = malloc(capacity);
    if (result == NULL) {
        snprintf(error, error_size, "Out of memory while resolving template.");
        return NULL;
    }
    length = 0;
    for (i = 0; text[i] != '\0'; ++i) {
        if (text[i] == '<') {
            char name[256];
            const char *value;
            size_t start;
            size_t end;
            size_t name_len;
            size_t value_len;

            start = i + 1;
            end = start;
            while (text[end] != '\0' && text[end] != '>') {
                end++;
            }
            if (text[end] != '>') {
                free(result);
                snprintf(error, error_size, "Unclosed placeholder.");
                return NULL;
            }
            name_len = end - start;
            if (name_len == 0 || name_len >= sizeof(name)) {
                free(result);
                snprintf(error, error_size, "Invalid placeholder size.");
                return NULL;
            }
            memcpy(name, text + start, name_len);
            name[name_len] = '\0';
            value = kc_flow_resolve_scope_value(name, flow_params, node_params);
            if (value == NULL) {
                free(result);
                snprintf(error, error_size, "Unable to resolve: <%.180s>", name);
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
        result[length++] = text[i];
    }
    result[length] = '\0';
    return result;
}

/**
 * Collects one node's effective params in local scope order.
 * @param model Parsed model.
 * @param node Source node.
 * @param flow_params Current flow scope.
 * @param effective_params Output node scope.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_collect_node_params(
    const kc_flow_model *model,
    const kc_flow_node *node,
    const kc_flow_overrides *flow_params,
    kc_flow_overrides *effective_params,
    char *error,
    size_t error_size
) {
    size_t i;

    (void)model;
    kc_flow_overrides_init(effective_params);
    for (i = 0; i < node->params.count; ++i) {
        char *resolved;

        resolved = kc_flow_resolve_template(
            node->params.records[i].value,
            flow_params,
            effective_params,
            error,
            error_size
        );
        if (resolved == NULL) {
            kc_flow_overrides_free(effective_params);
            return -1;
        }
        if (kc_flow_overrides_add(
                effective_params,
                node->params.records[i].key,
                resolved
            ) != 0) {
            free(resolved);
            kc_flow_overrides_free(effective_params);
            snprintf(error, error_size, "Unable to store node parameter.");
            return -1;
        }
        free(resolved);
    }
    return 0;
}

/**
 * Marks reachable nodes and detects cycles.
 * @param model Parsed model.
 * @param node Node under inspection.
 * @param seen Reachability marks.
 * @param stack Recursion stack marks.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_validate_node_walk(
    const kc_flow_model *model,
    const kc_flow_node *node,
    unsigned char *seen,
    unsigned char *stack,
    char *error,
    size_t error_size
) {
    size_t i;
    size_t index;

    index = (size_t)(node - model->nodes);
    if (stack[index]) {
        snprintf(error, error_size, "Flow contains one cycle.");
        return -1;
    }
    if (seen[index]) {
        return 0;
    }
    seen[index] = 1;
    stack[index] = 1;
    for (i = 0; i < node->links.count; ++i) {
        const kc_flow_node *child;

        child = kc_flow_model_find_node(model, node->links.values[i]);
        if (child == NULL) {
            snprintf(error, error_size, "Unknown node link: %s", node->links.values[i]);
            return -1;
        }
        if (kc_flow_validate_node_walk(model, child, seen, stack, error, error_size) != 0) {
            return -1;
        }
    }
    stack[index] = 0;
    return 0;
}

/**
 * Validates one parsed model.
 * @param model Parsed model.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_validate_model(const kc_flow_model *model, char *error, size_t error_size) {
    unsigned char seen[KC_FLOW_MAX_NODES];
    unsigned char stack[KC_FLOW_MAX_NODES];
    size_t i;

    if (model->id == NULL || model->id[0] == '\0') {
        snprintf(error, error_size, "Missing required key: flow.id");
        return -1;
    }
    if (model->entry_links.count == 0) {
        snprintf(error, error_size, "Missing required key: flow.link");
        return -1;
    }
    for (i = 0; i < model->node_count; ++i) {
        const kc_flow_node *node;

        node = &model->nodes[i];
        if ((node->file == NULL || node->file[0] == '\0') &&
                (node->exec == NULL || node->exec[0] == '\0') &&
                node->links.count == 0) {
            snprintf(error, error_size, "Node must define file, exec, or link.");
            return -1;
        }
    }
    memset(seen, 0, sizeof(seen));
    memset(stack, 0, sizeof(stack));
    for (i = 0; i < model->entry_links.count; ++i) {
        const kc_flow_node *entry;

        entry = kc_flow_model_find_node(model, model->entry_links.values[i]);
        if (entry == NULL) {
            snprintf(error, error_size, "Unknown flow link: %s", model->entry_links.values[i]);
            return -1;
        }
        if (kc_flow_validate_node_walk(model, entry, seen, stack, error, error_size) != 0) {
            return -1;
        }
    }
    return 0;
}
