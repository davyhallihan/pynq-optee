// SPDX-License-Identifier: BSD-2-Clause
/*
 * Secure Switch Benchmark — Trusted Application
 *
 * Commands:
 *   CMD_READ      — Returns stub value (actual MMIO read needs PTA)
 *   CMD_BENCHMARK — Same as CMD_READ but with two output params for
 *                   future use when converted to PTA with cycle counting
 *
 * All timing is done from the CA side via clock_gettime.
 */

#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <secure_switch_ta.h>

TEE_Result TA_CreateEntryPoint(void)
{
	DMSG("secure_switch TA created");
	return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void) {}

TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
				    TEE_Param __unused params[4],
				    void __unused **sess_ctx)
{
	uint32_t exp = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
				       TEE_PARAM_TYPE_NONE,
				       TEE_PARAM_TYPE_NONE,
				       TEE_PARAM_TYPE_NONE);
	if (param_types != exp)
		return TEE_ERROR_BAD_PARAMETERS;
	return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void __unused *sess_ctx) {}

/*
 * CMD_READ: Read switch state (stub — returns 0 until PTA conversion).
 */
static TEE_Result cmd_read(uint32_t param_types, TEE_Param params[4])
{
	uint32_t exp = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
				       TEE_PARAM_TYPE_NONE,
				       TEE_PARAM_TYPE_NONE,
				       TEE_PARAM_TYPE_NONE);
	if (param_types != exp)
		return TEE_ERROR_BAD_PARAMETERS;

	params[0].value.a = 0;
	return TEE_SUCCESS;
}

/*
 * CMD_BENCHMARK: Same interface as the future PTA version.
 *   out: params[0].value.a = switch state (stub: 0)
 *        params[0].value.b = 0 (will be AXI cycles in PTA)
 *        params[1].value.a = 0 (will be total TA cycles in PTA)
 *        params[1].value.b = 0 (reserved)
 */
static TEE_Result cmd_benchmark(uint32_t param_types, TEE_Param params[4])
{
	uint32_t exp = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
				       TEE_PARAM_TYPE_VALUE_OUTPUT,
				       TEE_PARAM_TYPE_NONE,
				       TEE_PARAM_TYPE_NONE);
	if (param_types != exp)
		return TEE_ERROR_BAD_PARAMETERS;

	params[0].value.a = 0;
	params[0].value.b = 0;
	params[1].value.a = 0;
	params[1].value.b = 0;
	return TEE_SUCCESS;
}

TEE_Result TA_InvokeCommandEntryPoint(void __unused *sess_ctx,
				      uint32_t cmd_id, uint32_t param_types,
				      TEE_Param params[4])
{
	switch (cmd_id) {
	case TA_SECURE_SWITCH_CMD_READ:
		return cmd_read(param_types, params);
	case TA_SECURE_SWITCH_CMD_BENCHMARK:
		return cmd_benchmark(param_types, params);
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}
}
