/**
 * link.h
 * Summary: Flow endpoint and link graph helper API.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#ifndef KC_FLOW_LINK_H
#define KC_FLOW_LINK_H

#include <stddef.h>

#include "model.h"

typedef enum kc_flow_endpoint_kind {
    KC_FLOW_ENDPOINT_INVALID = 0,
    KC_FLOW_ENDPOINT_INPUT,
    KC_FLOW_ENDPOINT_OUTPUT,
    KC_FLOW_ENDPOINT_NODE_IN,
    KC_FLOW_ENDPOINT_NODE_OUT
} kc_flow_endpoint_kind;

typedef struct kc_flow_endpoint {
    kc_flow_endpoint_kind kind;
    char node_id[128];
    char field_id[128];
} kc_flow_endpoint;

typedef struct kc_flow_link_entry {
    kc_flow_endpoint from;
    kc_flow_endpoint to;
} kc_flow_link_entry;

int kc_flow_collect_node_ids(
    const kc_flow_model *model,
    char node_ids[][128],
    size_t *count,
    char *error,
    size_t error_size
);

int kc_flow_collect_links(
    const kc_flow_model *model,
    char node_ids[][128],
    size_t node_count,
    kc_flow_link_entry *links,
    size_t *link_count,
    char *error,
    size_t error_size
);

int kc_flow_parse_link_endpoint(
    const char *text,
    kc_flow_endpoint *endpoint
);

int kc_flow_validate_link_endpoint_pair(
    const kc_flow_model *model,
    char node_ids[][128],
    size_t node_count,
    const kc_flow_endpoint *from,
    const kc_flow_endpoint *to,
    char *error,
    size_t error_size
);

int kc_flow_detect_cycle(
    const kc_flow_model *model,
    char node_ids[][128],
    size_t node_count,
    char *error,
    size_t error_size
);

#endif
