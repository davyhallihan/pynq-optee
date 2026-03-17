// SPDX-License-Identifier: BSD-2-Clause
// Benchmarks involving AXI peripheral reads (secure and non-secure)

#include "benchmark.h"


// ============================================================================
// Benchmark: SMC + secure AXI read (PTA_CMD_AXI_READ)
//
// Same world-switch path as above, but the PTA also does a single MMIO
// read of the secure-only FPGA peripheral (io_read32). The PTA uses the
// ARM cycle counter (PMCCNTR) to report how many CPU cycles the read took,
// giving us both wall-clock time (from Linux) and cycle-accurate time
// (from the secure side).
//
// Returns three measurements per iteration:
//   - wall-clock round-trip (ns, from clock_gettime)
//   - AXI read cycles (PMCCNTR delta around just the io_read32)
//   - total PTA cycles (PMCCNTR delta for the entire command handler)
// ============================================================================

void bench_smc_axi(TEEC_Session *sess, int n)
{
	TEEC_Result res;
	TEEC_Operation op;
	uint32_t err_origin;
	struct timespec t0, t1;
	uint64_t *rtt_ns;
	uint32_t *axi_cyc, *ta_cyc;
	int i;

	rtt_ns  = calloc(n, sizeof(uint64_t));
	axi_cyc = calloc(n, sizeof(uint32_t));
	ta_cyc  = calloc(n, sizeof(uint32_t));
	if (!rtt_ns || !axi_cyc || !ta_cyc) errx(1, "malloc failed");

	for (i = 0; i < n; i++) {
		memset(&op, 0, sizeof(op));
		op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_OUTPUT,
						 TEEC_VALUE_OUTPUT,
						 TEEC_NONE, TEEC_NONE);
		clock_gettime(CLOCK_MONOTONIC, &t0);
		res = TEEC_InvokeCommand(sess, PTA_CMD_AXI_READ, &op, &err_origin);
		clock_gettime(CLOCK_MONOTONIC, &t1);
		if (res != TEEC_SUCCESS)
			errx(1, "CMD_AXI_READ failed: 0x%x origin 0x%x", res, err_origin);
		rtt_ns[i]  = timespec_to_ns(&t1) - timespec_to_ns(&t0);
		axi_cyc[i] = op.params[0].value.b;
		ta_cyc[i]  = op.params[1].value.a;
	}

	if (csv_mode) {
		printf("smc_axi_iter,smc_s_axi_rtt_ns,s_axi_cyc,ta_total_cyc\n");
		for (i = 0; i < n; i++)
			printf("%d,%lu,%u,%u\n", i,
			       (unsigned long)rtt_ns[i], axi_cyc[i], ta_cyc[i]);
	} else {
		struct stats s_rtt = compute_stats_u64(rtt_ns, n);
		struct stats s_axi = compute_stats_u32(axi_cyc, n);
		struct stats s_ta  = compute_stats_u32(ta_cyc, n);

		printf("\n=== SMC + Secure AXI Read (%d iterations) ===\n", n);
		printf("Wall-clock RTT:\n");
		print_stats("ns", &s_rtt);
		printf("Secure AXI cycles (PMCCNTR delta around io_read32):\n");
		print_stats("cyc", &s_axi);
		printf("Total PTA cycles (entry to exit):\n");
		print_stats("cyc", &s_ta);
	}

	free(rtt_ns);
	free(axi_cyc);
	free(ta_cyc);
}


// ============================================================================
// Benchmark: Non-secure AXI read (via /dev/mem, no SMC)
//
// Reads the non-secure FPGA peripheral directly from Linux userspace by
// mmap'ing /dev/mem. No OP-TEE, no SMC, no world switch. This is the
// baseline: the raw cost of a CPU-initiated AXI read through the PS-PL
// bridge, as seen from userspace.
//
// Requires CONFIG_STRICT_DEVMEM=n in the kernel config.
// ============================================================================

void bench_ns_axi(int n)
{
	int fd;
	volatile uint32_t *regs;
	struct timespec t0, t1;
	uint64_t *rtt_ns;
	int i;

	rtt_ns = calloc(n, sizeof(uint64_t));
	if (!rtt_ns) errx(1, "malloc failed");

	fd = open("/dev/mem", O_RDONLY | O_SYNC);
	if (fd < 0)
		err(1, "open /dev/mem (need root or mmap permissions)");

	regs = mmap(NULL, NS_SWITCH_SIZE, PROT_READ, MAP_SHARED,
		    fd, NS_SWITCH_ADDR);
	if (regs == MAP_FAILED)
		err(1, "mmap NS switch at 0x%lx", (unsigned long)NS_SWITCH_ADDR);

	for (i = 0; i < n; i++) {
		clock_gettime(CLOCK_MONOTONIC, &t0);
		(void)regs[0];  /* volatile read of switch register */
		clock_gettime(CLOCK_MONOTONIC, &t1);
		rtt_ns[i] = timespec_to_ns(&t1) - timespec_to_ns(&t0);
	}

	munmap((void *)regs, NS_SWITCH_SIZE);
	close(fd);

	if (csv_mode) {
		printf("ns_axi_iter,ns_axi_rtt_ns\n");
		for (i = 0; i < n; i++)
			printf("%d,%lu\n", i, (unsigned long)rtt_ns[i]);
	} else {
		struct stats s = compute_stats_u64(rtt_ns, n);
		printf("\n=== Non-Secure AXI Read (%d iterations) ===\n", n);
		print_stats("ns", &s);
	}

	free(rtt_ns);
}


// ============================================================================
// Benchmark: Multi-read sweep (PTA_CMD_AXI_READ_N)
//
// Calls the PTA with varying numbers of io_read32() per invocation
// (1, 2, 4, 8, 16). By fitting a line through cycles-vs-num_reads, we
// extract the marginal cost of a single AXI read in cycles, separated
// from the fixed overhead of the SMC + PTA dispatch.
// ============================================================================

void bench_multi_read(TEEC_Session *sess, int iterations)
{
	static const int read_counts[] = { 1, 2, 4, 8, 16 };
	int num_steps = sizeof(read_counts) / sizeof(read_counts[0]);
	TEEC_Result res;
	TEEC_Operation op;
	uint32_t err_origin;
	uint32_t *cyc;
	int s, i;

	cyc = calloc(iterations, sizeof(uint32_t));
	if (!cyc) errx(1, "malloc failed");

	if (csv_mode)
		printf("num_reads,iter,total_cyc\n");
	else
		printf("\n=== Multi-Read Sweep (%d iterations per step) ===\n",
		       iterations);

	for (s = 0; s < num_steps; s++) {
		int nr = read_counts[s];

		for (i = 0; i < iterations; i++) {
			memset(&op, 0, sizeof(op));
			op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT,
							 TEEC_VALUE_OUTPUT,
							 TEEC_NONE,
							 TEEC_NONE);
			op.params[0].value.a = nr;

			res = TEEC_InvokeCommand(sess, PTA_CMD_AXI_READ_N,
						 &op, &err_origin);
			if (res != TEEC_SUCCESS)
				errx(1, "CMD_AXI_READ_N(%d) failed: 0x%x origin 0x%x",
				     nr, res, err_origin);
			cyc[i] = op.params[1].value.a;
		}

		if (csv_mode) {
			for (i = 0; i < iterations; i++)
				printf("%d,%d,%u\n", nr, i, cyc[i]);
		} else {
			struct stats st = compute_stats_u32(cyc, iterations);
			printf("  %2d reads: avg %.1f cyc  stddev %.1f  "
			       "min %lu  max %lu\n",
			       nr, st.avg, st.stddev,
			       (unsigned long)st.min, (unsigned long)st.max);
		}
	}

	if (!csv_mode) {
		printf("  (slope of avg vs num_reads = marginal per-read cost)\n");
	}

	free(cyc);
}
