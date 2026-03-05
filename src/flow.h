/**
 * flow.h
 * Summary: Flow execution helper API.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#ifndef KC_FLOW_FLOW_H
#define KC_FLOW_FLOW_H

#include <stddef.h>

#include "link.h"
#include "runtime.h"

int kc_flow_seed_parent_inputs(const kc_flow_overrides *overrides,
                               kc_flow_overrides *values,
                               char *error,
                               size_t error_size);

int kc_flow_run_node(const kc_flow_model *model,
                     const char *flow_dir,
                     const char *node_id,
                     const kc_flow_overrides *node_inputs,
                     kc_flow_overrides *node_outputs,
                     char *error,
                     size_t error_size);

int kc_flow_collect_node_outputs(const kc_flow_link_entry *links,
                                 size_t link_count,
                                 const char *node_id,
                                 const kc_flow_overrides *node_outputs,
                                 kc_flow_overrides *values,
                                 char *error,
                                 size_t error_size);

int kc_flow_resolve_final_outputs(const kc_flow_model *model,
                                  const kc_flow_link_entry *links,
                                  size_t link_count,
                                  const kc_flow_overrides *values,
                                  kc_flow_overrides *outputs,
                                  char *error,
                                  size_t error_size);

int kc_flow_find_node_record_index(const kc_flow_model *model,
                                   const char *node_id);

#endif
