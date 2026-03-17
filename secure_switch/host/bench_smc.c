// SPDX-License-Identifier: BSD-2-Clause
// TEE-only benchmarks: pure SMC round-trip, throughput, session open/close

#include "benchmark.h"


// ============================================================================
// Benchmark: SMC round-trip (PTA_CMD_NOP)
//
// Measures the pure world-switch cost. The PTA does zero work in secure
// world -- it just returns. So the time is entirely:
//   Linux ioctl -> TEE driver -> SMC trap -> OP-TEE dispatch -> SMC return
// ============================================================================

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


// ============================================================================
// Benchmark: SMC throughput (batch PTA_CMD_NOP)
//
// Fires N back-to-back NOPs with a single timer around the whole batch.
// Gives aggregate throughput (SMCs/sec) rather than per-call latency.
// ============================================================================

void bench_throughput(TEEC_Session *sess, int n)
{
	TEEC_Result res;
	TEEC_Operation op;
	uint32_t err_origin;
	struct timespec t0, t1;
	uint64_t total_ns;
	int i;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE,
					 TEEC_NONE, TEEC_NONE);

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (i = 0; i < n; i++) {
		res = TEEC_InvokeCommand(sess, PTA_CMD_NOP, &op, &err_origin);
		if (res != TEEC_SUCCESS)
			errx(1, "CMD_NOP failed: 0x%x origin 0x%x", res, err_origin);
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);

	total_ns = timespec_to_ns(&t1) - timespec_to_ns(&t0);

	if (csv_mode) {
		printf("throughput_n,total_ns,avg_ns,smcs_per_sec\n");
		printf("%d,%lu,%.1f,%.0f\n", n, (unsigned long)total_ns,
		       (double)total_ns / n,
		       (double)n / ((double)total_ns / 1e9));
	} else {
		printf("\n=== SMC Throughput (%d back-to-back CMD_NOP) ===\n", n);
		printf("  total: %lu ns\n", (unsigned long)total_ns);
		printf("  avg:   %.1f ns/call\n", (double)total_ns / n);
		printf("  rate:  %.0f SMCs/sec\n",
		       (double)n / ((double)total_ns / 1e9));
	}
}


// ============================================================================
// Benchmark: TEE session open/close
//
// Measures the cost of establishing and tearing down an OP-TEE session.
// Each iteration opens a fresh session to the PTA and immediately closes it.
// ============================================================================

void bench_session(TEEC_Context *ctx, int n)
{
	TEEC_Result res;
	TEEC_Session sess;
	TEEC_UUID uuid = PTA_BENCHMARK_UUID;
	uint32_t err_origin;
	struct timespec t0, t1;
	uint64_t *open_ns, *close_ns;
	int i;

	open_ns  = calloc(n, sizeof(uint64_t));
	close_ns = calloc(n, sizeof(uint64_t));
	if (!open_ns || !close_ns) errx(1, "malloc failed");

	for (i = 0; i < n; i++) {
		clock_gettime(CLOCK_MONOTONIC, &t0);
		res = TEEC_OpenSession(ctx, &sess, &uuid,
				       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
		clock_gettime(CLOCK_MONOTONIC, &t1);
		if (res != TEEC_SUCCESS)
			errx(1, "TEEC_OpenSession failed: 0x%x origin 0x%x",
			     res, err_origin);
		open_ns[i] = timespec_to_ns(&t1) - timespec_to_ns(&t0);

		clock_gettime(CLOCK_MONOTONIC, &t0);
		TEEC_CloseSession(&sess);
		clock_gettime(CLOCK_MONOTONIC, &t1);
		close_ns[i] = timespec_to_ns(&t1) - timespec_to_ns(&t0);
	}

	if (csv_mode) {
		printf("session_iter,open_ns,close_ns\n");
		for (i = 0; i < n; i++)
			printf("%d,%lu,%lu\n", i,
			       (unsigned long)open_ns[i],
			       (unsigned long)close_ns[i]);
	} else {
		struct stats s_open  = compute_stats_u64(open_ns, n);
		struct stats s_close = compute_stats_u64(close_ns, n);

		printf("\n=== Session Open/Close (%d iterations) ===\n", n);
		printf("OpenSession:\n");
		print_stats("ns", &s_open);
		printf("CloseSession:\n");
		print_stats("ns", &s_close);
	}

	free(open_ns);
	free(close_ns);
}
