// SPDX-License-Identifier: BSD-2-Clause
/*
 * PTA Benchmark — Pseudo Trusted Application for Zynq-7000 (AArch32)
 *
 * Runs inside OP-TEE core to perform secure-world MMIO reads of the
 * AXI switch peripheral with ARM PMU cycle counting. This avoids the
 * overhead of a user-space TA and gives direct access to physical I/O.
 *
 * Build: compile into OP-TEE OS by adding this file to a sub.mk and
 *        setting CFG_SWITCH_BASE to the secure peripheral's physical address.
 *
 * Required OP-TEE build flags:
 *   CFG_SWITCH_BASE=0x43C00000   (update to match Vivado assign_bd_address output)
 */

#include <compiler.h>
#include <io.h>
#include <kernel/pseudo_ta.h>
#include <mm/core_memprot.h>
#include <string.h>
#include <trace.h>

#define PTA_NAME "benchmark.pta"

#define PTA_BENCHMARK_UUID \
	{ 0xb2c3d4e5, 0x6789, 0xabcd, \
		{ 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd } }

#define PTA_CMD_NOP		0
#define PTA_CMD_AXI_READ	1
#define PTA_CMD_AXI_READ_N	2

#ifndef CFG_SWITCH_BASE
#error "CFG_SWITCH_BASE must be defined (secure switch peripheral physical address)"
#endif

#define SWITCH_PHYS	((paddr_t)CFG_SWITCH_BASE)
#define SWITCH_SIZE	0x1000

/*
 * Cycle counter helpers for Cortex-A9 (AArch32)
 *
 * We enable PMCCNTR counting via CP15, read it before/after the AXI
 * access, and return the delta. The PTA runs in secure SVC mode so
 * we have full access to the PMU registers.
 *
 * Note: On Cortex-A9, PMCCNTR increments every 64th clock cycle by
 * default when D bit is set in PMCR. We clear the D bit so it counts
 * every cycle.
 */
static inline void pmu_enable(void)
{
	uint32_t val;

	/* Read PMCR, set E (enable), C (reset cycle counter), clear D (every-cycle mode) */
	asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(val));
	val |= (1 << 0) | (1 << 2);   /* E | C */
	val &= ~(1 << 3);             /* clear D — count every cycle */
	asm volatile("mcr p15, 0, %0, c9, c12, 0" : : "r"(val));

	/* Enable cycle counter (PMCNTENSET bit 31) */
	asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"((uint32_t)(1U << 31)));

	/* Ensure writes complete before reading */
	asm volatile("isb");
}

static inline uint32_t read_cycles(void)
{
	uint32_t cnt;

	asm volatile("isb" ::: "memory");
	asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(cnt));
	return cnt;
}

/*
 * Open session — map the secure switch peripheral.
 */
static TEE_Result pta_open(uint32_t param_types __unused,
			   TEE_Param params[4] __unused,
			   void **sess_ctx)
{
	vaddr_t va;

	va = (vaddr_t)phys_to_virt(SWITCH_PHYS, MEM_AREA_IO_SEC, SWITCH_SIZE);
	if (!va) {
		EMSG("phys_to_virt(0x%" PRIxPA ") failed — is the address in "
		     "the platform secure I/O map?", SWITCH_PHYS);
		return TEE_ERROR_GENERIC;
	}

	*sess_ctx = (void *)va;
	DMSG("Mapped secure switch at PA 0x%" PRIxPA " → VA %p",
	     SWITCH_PHYS, (void *)va);
	return TEE_SUCCESS;
}

/*
 * CMD_NOP — return immediately.
 */
static TEE_Result cmd_nop(uint32_t param_types,
			  TEE_Param params[4] __unused)
{
	if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
					   TEE_PARAM_TYPE_NONE,
					   TEE_PARAM_TYPE_NONE,
					   TEE_PARAM_TYPE_NONE))
		return TEE_ERROR_BAD_PARAMETERS;

	return TEE_SUCCESS;
}

/*
 * CMD_AXI_READ — MMIO read with cycle counting.
 */
static TEE_Result cmd_axi_read(uint32_t param_types,
			       TEE_Param params[4],
			       vaddr_t switch_va)
{
	uint32_t c_start, c_axi_start, c_axi_end, c_end;
	uint32_t val;

	if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
					   TEE_PARAM_TYPE_VALUE_OUTPUT,
					   TEE_PARAM_TYPE_NONE,
					   TEE_PARAM_TYPE_NONE))
		return TEE_ERROR_BAD_PARAMETERS;

	pmu_enable();

	c_start = read_cycles();

	c_axi_start = read_cycles();
	val = io_read32(switch_va);
	c_axi_end = read_cycles();

	c_end = read_cycles();

	params[0].value.a = val;
	params[0].value.b = c_axi_end - c_axi_start;
	params[1].value.a = c_end - c_start;
	params[1].value.b = 0;

	return TEE_SUCCESS;
}

/*
 * CMD_AXI_READ_N — multiple consecutive MMIO reads, one SMC.
 *
 * in:  params[0].value.a = number of reads (1..16)
 * out: params[0].value.b = last read value
 *      params[1].value.a = total cycles for all N reads
 *      params[1].value.b = 0
 */
static TEE_Result cmd_axi_read_n(uint32_t param_types,
				 TEE_Param params[4],
				 vaddr_t switch_va)
{
	uint32_t c_start, c_end;
	uint32_t n, val = 0;
	uint32_t i;

	if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
					   TEE_PARAM_TYPE_VALUE_OUTPUT,
					   TEE_PARAM_TYPE_NONE,
					   TEE_PARAM_TYPE_NONE))
		return TEE_ERROR_BAD_PARAMETERS;

	n = params[0].value.a;
	if (n < 1 || n > 16)
		return TEE_ERROR_BAD_PARAMETERS;

	pmu_enable();

	c_start = read_cycles();
	for (i = 0; i < n; i++)
		val = io_read32(switch_va);
	c_end = read_cycles();

	params[0].value.b = val;
	params[1].value.a = c_end - c_start;
	params[1].value.b = 0;

	return TEE_SUCCESS;
}

static TEE_Result pta_invoke(void *sess_ctx,
			     uint32_t cmd_id, uint32_t param_types,
			     TEE_Param params[4])
{
	vaddr_t switch_va = (vaddr_t)sess_ctx;

	switch (cmd_id) {
	case PTA_CMD_NOP:
		return cmd_nop(param_types, params);
	case PTA_CMD_AXI_READ:
		return cmd_axi_read(param_types, params, switch_va);
	case PTA_CMD_AXI_READ_N:
		return cmd_axi_read_n(param_types, params, switch_va);
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}
}

pseudo_ta_register(.uuid = PTA_BENCHMARK_UUID,
		   .name = PTA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .open_session_entry_point = pta_open,
		   .invoke_command_entry_point = pta_invoke);
