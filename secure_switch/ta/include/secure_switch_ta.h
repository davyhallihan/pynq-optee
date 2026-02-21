/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Secure Switch Benchmark — Shared TA/CA header
 */
#ifndef TA_SECURE_SWITCH_H
#define TA_SECURE_SWITCH_H

#define TA_SECURE_SWITCH_UUID \
	{ 0xa1b2c3d4, 0x5678, 0x9abc, \
		{ 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc } }

/* Commands */
#define TA_SECURE_SWITCH_CMD_READ		0  /* Read switch state */
#define TA_SECURE_SWITCH_CMD_BENCHMARK		1  /* Read switch + return timing */

#endif /* TA_SECURE_SWITCH_H */
