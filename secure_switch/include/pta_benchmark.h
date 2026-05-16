// Shared header between the PTA (secure world) and the host app (Linux).
// Both sides include this to agree on the UUID and command IDs.

#ifndef PTA_BENCHMARK_H
#define PTA_BENCHMARK_H

#define PTA_BENCHMARK_UUID \
	{ 0xb2c3d4e5, 0x6789, 0xabcd, \
		{ 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd } }

// PTA_CMD_NOP: Returns immediately. Measures pure SMC round-trip overhead.
//   in:  (none)
//   out: (none)
#define PTA_CMD_NOP		0

// PTA_CMD_AXI_READ: Single MMIO read of the secure switch register.
//   in:  (none)
//   out: value[0].a = register value (sw[1:0])
//        value[0].b = AXI read cycles (PMCCNTR delta around io_read32)
//        value[1].a = total PTA command cycles
//        value[1].b = 0 (reserved)
#define PTA_CMD_AXI_READ	1

// PTA_CMD_AXI_READ_N: N consecutive MMIO reads in one SMC call.
//   in:  value[0].a = number of reads (1..16)
//   out: value[0].b = last read value
//        value[1].a = total cycles for all N reads
//        value[1].b = 0 (reserved)
#define PTA_CMD_AXI_READ_N	2

#endif // PTA_BENCHMARK_H
