// SPDX-License-Identifier: BSD-2-Clause
/*
 * Secure Switch Benchmark — Trusted Application
 *
 * Reads a TrustZone-protected AXI switch peripheral and provides timing
 * instrumentation for benchmarking SMC call overhead and AXI access latency.
 *
 * Commands:
 *   CMD_READ      — Read switch state, return value
 *   CMD_BENCHMARK — Read switch state, return value + secure-world cycle counts
 *
 * The ARM performance monitor cycle counter (PMCCNTR) is used for timing.
 * On Cortex-A9 (Zynq-7000) and Cortex-A53 (ZynqMP), this gives cycle-accurate
 * measurements of the AXI read time within the secure world.
 */

#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <secure_switch_ta.h>

/*
 * Physical address of the AXI switch reader peripheral.
 * Differs per platform:
 *   Zynq-7000 (PYNQ-Z2): GP0 range, typically 0x43C00000
 *   ZynqMP (AUP-ZU3):    HPM0 range, typically 0xA0000000
 *
 * Override at build time: CFLAGS += -DSECURE_SWITCH_PHYS=0xA0000000
 */
#ifndef SECURE_SWITCH_PHYS
#define SECURE_SWITCH_PHYS	0x43C00000
#endif
#define SECURE_SWITCH_SIZE	0x1000

/* ---------- ARM cycle counter helpers ---------- */

static inline void enable_cycle_counter(void)
{
	uint32_t val;
	/* Enable all counters (PMCR.E) */
	asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(val));
	val |= 1;  /* set E bit */
	asm volatile("mcr p15, 0, %0, c9, c12, 0" :: "r"(val));
	/* Enable cycle counter (PMCNTENSET.C) */
	asm volatile("mcr p15, 0, %0, c9, c12, 1" :: "r"(1U << 31));
}

static inline uint32_t read_cycle_counter(void)
{
	uint32_t val;
	asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(val));
	return val;
}

/* ---------- TA entry points ---------- */

TEE_Result TA_CreateEntryPoint(void)
{
	DMSG("secure_switch TA created");
	enable_cycle_counter();
	return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void)
{
	DMSG("secure_switch TA destroyed");
}

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

/* ---------- Switch reading ---------- */

static volatile uint32_t *map_switch_register(void)
{
	volatile uint32_t *reg;

	reg = (volatile uint32_t *)phys_to_virt_io(SECURE_SWITCH_PHYS,
						   SECURE_SWITCH_SIZE);
	if (!reg)
		EMSG("Failed to map switch register at 0x%08x",
		     SECURE_SWITCH_PHYS);
	return reg;
}

/*
 * CMD_READ: Simple switch read.
 *   out: params[0].value.a = switch state (bits [1:0])
 */
static TEE_Result cmd_read(uint32_t param_types, TEE_Param params[4])
{
	uint32_t exp = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
				       TEE_PARAM_TYPE_NONE,
				       TEE_PARAM_TYPE_NONE,
				       TEE_PARAM_TYPE_NONE);
	volatile uint32_t *sw_reg;

	if (param_types != exp)
		return TEE_ERROR_BAD_PARAMETERS;

	sw_reg = map_switch_register();
	if (!sw_reg)
		return TEE_ERROR_GENERIC;

	params[0].value.a = *sw_reg & 0x3;
	return TEE_SUCCESS;
}

/*
 * CMD_BENCHMARK: Switch read with cycle-count timing.
 *   out: params[0].value.a = switch state (bits [1:0])
 *        params[0].value.b = AXI read cycles (secure-world, just the MMIO read)
 *        params[1].value.a = total TA command cycles (entry to exit)
 *        params[1].value.b = 0 (reserved)
 */
static TEE_Result cmd_benchmark(uint32_t param_types, TEE_Param params[4])
{
	uint32_t exp = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
				       TEE_PARAM_TYPE_VALUE_OUTPUT,
				       TEE_PARAM_TYPE_NONE,
				       TEE_PARAM_TYPE_NONE);
	volatile uint32_t *sw_reg;
	uint32_t t_start, t_axi_start, t_axi_end, t_end;
	uint32_t val;

	t_start = read_cycle_counter();

	if (param_types != exp)
		return TEE_ERROR_BAD_PARAMETERS;

	sw_reg = map_switch_register();
	if (!sw_reg)
		return TEE_ERROR_GENERIC;

	/* Time just the AXI read */
	t_axi_start = read_cycle_counter();
	val = *sw_reg;
	t_axi_end = read_cycle_counter();

	t_end = read_cycle_counter();

	params[0].value.a = val & 0x3;
	params[0].value.b = t_axi_end - t_axi_start;  /* AXI read cycles */
	params[1].value.a = t_end - t_start;           /* Total TA cycles */
	params[1].value.b = 0;

	IMSG("Benchmark: AXI=%u cycles, total_TA=%u cycles, sw=0x%x",
	     t_axi_end - t_axi_start, t_end - t_start, val & 0x3);

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
