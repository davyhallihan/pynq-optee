/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * PTA Benchmark — Shared PTA/CA header
 *
 * Defines the UUID and command IDs for the benchmark Pseudo Trusted
 * Application that runs inside OP-TEE core.
 */
#ifndef PTA_BENCHMARK_H
#define PTA_BENCHMARK_H

#define PTA_BENCHMARK_UUID \
	{ 0xb2c3d4e5, 0x6789, 0xabcd, \
		{ 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd } }

/*
 * PTA_CMD_NOP - No-op command
 *
 * Returns immediately. Used to measure pure SMC round-trip overhead
 * (context switch NS → S → NS) with no work done in secure world.
 *
 * [in]  params: none
 * [out] params: none
 */
#define PTA_CMD_NOP		0

/*
 * PTA_CMD_AXI_READ - Secure AXI MMIO read with cycle counting
 *
 * Reads the secure switch register via MMIO and returns the value
 * along with cycle counts from the ARM Performance Monitor (PMCCNTR).
 *
 * [out] value[0].a: switch register value
 * [out] value[0].b: AXI read cycles (PMCCNTR delta around io_read32)
 * [out] value[1].a: total PTA command cycles (entry to exit)
 * [out] value[1].b: reserved (0)
 */
#define PTA_CMD_AXI_READ	1

/*
 * PTA_CMD_AXI_READ_N - Multiple consecutive MMIO reads in one SMC
 *
 * Performs N consecutive io_read32() calls in a single secure-world
 * invocation and returns total cycle count. Used to extract the
 * marginal per-read cost (slope of cycles vs N).
 *
 * [in]  value[0].a: number of reads to perform (1..16)
 * [out] value[0].b: last read value
 * [out] value[1].a: total cycles for all N reads
 * [out] value[1].b: reserved (0)
 */
#define PTA_CMD_AXI_READ_N	2

#endif /* PTA_BENCHMARK_H */
