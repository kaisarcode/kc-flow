/**
 * runtime.h
 * Summary: Atomic contract execution runtime and output materialization APIs.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#ifndef KC_FLOW_RUNTIME_H
#define KC_FLOW_RUNTIME_H

#include <stddef.h>

#include "model.h"

typedef struct kc_flow_run_output {
    char *stdout_text;
    char *stderr_text;
    int exit_code;
} kc_flow_run_output;

void kc_flow_run_output_free(kc_flow_run_output *output);

int kc_flow_run_contract(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *cfg_path,
    kc_flow_run_output *output,
    char *error,
    size_t error_size
);

int kc_flow_collect_contract_outputs(
    const kc_flow_model *model,
    const kc_flow_run_output *output,
    kc_flow_overrides *values,
    char *error,
    size_t error_size
);

int kc_flow_print_contract_outputs(const kc_flow_model *model, const kc_flow_run_output *output);

#endif
