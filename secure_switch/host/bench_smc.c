// SPDX-License-Identifier: BSD-2-Clause
// Benchmark: pure SMC round-trip (no AXI reads)

#include "benchmark.h"

void bench_smc(TEEC_Session *sess, int n)
{
	TEEC_Result res;
	TEEC_Operation op;
	uint32_t err_origin;
	struct timespec t0, t1;
	uint64_t *rtt_ns;
	int i;

	rtt_ns = calloc(n, sizeof(uint64_t));
	if (!rtt_ns) errx(1, "malloc failed");

	for (i = 0; i < n; i++) {
		memset(&op, 0, sizeof(op));
		op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE,
						 TEEC_NONE, TEEC_NONE);
		clock_gettime(CLOCK_MONOTONIC, &t0);
		res = TEEC_InvokeCommand(sess, PTA_CMD_NOP, &op, &err_origin);
		clock_gettime(CLOCK_MONOTONIC, &t1);
		if (res != TEEC_SUCCESS)
			errx(1, "CMD_NOP failed: 0x%x origin 0x%x", res, err_origin);
		rtt_ns[i] = timespec_to_ns(&t1) - timespec_to_ns(&t0);
	}

	if (csv_mode) {
		printf("smc_iter,smc_rtt_ns\n");
		for (i = 0; i < n; i++)
			printf("%d,%lu\n", i, (unsigned long)rtt_ns[i]);
	} else {
		struct stats s = compute_stats_u64(rtt_ns, n);
		printf("\n=== SMC Round-Trip (%d iterations) ===\n", n);
		print_stats("ns", &s);
	}

	free(rtt_ns);
}
