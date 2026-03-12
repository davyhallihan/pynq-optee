// SPDX-License-Identifier: BSD-2-Clause
/*
 * OP-TEE Benchmark Suite — Normal World Host Application
 *
 * Measures four latency components of trusted computing on Zynq SoCs:
 *
 *   1. smc_rtt       — Pure SMC round-trip (CMD_NOP), no work in secure world
 *   2. smc_s_axi_rtt — SMC + secure AXI read (CMD_AXI_READ), wall-clock
 *   3. s_axi_rtt     — Secure AXI read latency in cycles (from PTA PMCCNTR)
 *   4. ns_axi_rtt    — Non-secure AXI read via /dev/mem mmap (no SMC)
 *
 * Usage:
 *   optee_benchmark [--smc N] [--smc-axi N] [--ns-axi N] [--all N] [--csv]
 */

#include <err.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <tee_client_api.h>
#include <pta_benchmark.h>

/* Non-secure switch peripheral address — set at compile time per board */
#ifndef NS_SWITCH_ADDR
#define NS_SWITCH_ADDR 0x40000000  /* PYNQ-Z2 default; override with -DNS_SWITCH_ADDR=... */
#endif

#define NS_SWITCH_SIZE 0x1000

static int csv_mode;

/* -------------------------------------------------------------------------- */
/* Helpers                                                                     */
/* -------------------------------------------------------------------------- */

static uint64_t timespec_to_ns(struct timespec *ts)
{
	return (uint64_t)ts->tv_sec * 1000000000ULL + ts->tv_nsec;
}

struct stats {
	uint64_t min;
	uint64_t max;
	double avg;
	double stddev;
};

static struct stats compute_stats_u64(const uint64_t *data, int n)
{
	struct stats s = { .min = ~0ULL, .max = 0 };
	double sum = 0, sum_sq = 0;
	int i;

	for (i = 0; i < n; i++) {
		if (data[i] < s.min) s.min = data[i];
		if (data[i] > s.max) s.max = data[i];
		sum += (double)data[i];
	}
	s.avg = sum / n;
	for (i = 0; i < n; i++) {
		double d = (double)data[i] - s.avg;
		sum_sq += d * d;
	}
	s.stddev = sqrt(sum_sq / n);
	return s;
}

static struct stats compute_stats_u32(const uint32_t *data, int n)
{
	uint64_t *tmp = malloc(n * sizeof(uint64_t));
	struct stats s;
	int i;

	if (!tmp) errx(1, "malloc failed");
	for (i = 0; i < n; i++)
		tmp[i] = data[i];
	s = compute_stats_u64(tmp, n);
	free(tmp);
	return s;
}

static void print_stats(const char *label, const char *unit, struct stats *s)
{
	(void)label;
	printf("  avg: %.1f %s    stddev: %.1f %s\n", s->avg, unit, s->stddev, unit);
	printf("  min: %lu %s     max: %lu %s\n",
	       (unsigned long)s->min, unit, (unsigned long)s->max, unit);
}

/* -------------------------------------------------------------------------- */
/* SMC Round-Trip (CMD_NOP)                                                    */
/* -------------------------------------------------------------------------- */

static void bench_smc(TEEC_Session *sess, int n)
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
		print_stats("smc_rtt", "ns", &s);
	}

	free(rtt_ns);
}

/* -------------------------------------------------------------------------- */
/* SMC + Secure AXI Read (CMD_AXI_READ)                                        */
/* -------------------------------------------------------------------------- */

static void bench_smc_axi(TEEC_Session *sess, int n)
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
		print_stats("smc_s_axi_rtt", "ns", &s_rtt);
		printf("Secure AXI cycles (PMCCNTR delta around io_read32):\n");
		print_stats("s_axi_cyc", "cyc", &s_axi);
		printf("Total PTA cycles (entry to exit):\n");
		print_stats("ta_total_cyc", "cyc", &s_ta);
	}

	free(rtt_ns);
	free(axi_cyc);
	free(ta_cyc);
}

/* -------------------------------------------------------------------------- */
/* Non-Secure AXI Read (via /dev/mem mmap)                                     */
/* -------------------------------------------------------------------------- */

static void bench_ns_axi(int n)
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
		print_stats("ns_axi_rtt", "ns", &s);
	}

	free(rtt_ns);
}

/* -------------------------------------------------------------------------- */
/* SMC Throughput (batch CMD_NOP)                                               */
/* -------------------------------------------------------------------------- */

static void bench_throughput(TEEC_Session *sess, int n)
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

/* -------------------------------------------------------------------------- */
/* Session Open/Close Timing                                                   */
/* -------------------------------------------------------------------------- */

static void bench_session(TEEC_Context *ctx, int n)
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
		print_stats("open", "ns", &s_open);
		printf("CloseSession:\n");
		print_stats("close", "ns", &s_close);
	}

	free(open_ns);
	free(close_ns);
}

/* -------------------------------------------------------------------------- */
/* Multi-Read Sweep (CMD_AXI_READ_N with varying N)                            */
/* -------------------------------------------------------------------------- */

static void bench_multi_read(TEEC_Session *sess, int iterations)
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

/* -------------------------------------------------------------------------- */
/* Usage / Main                                                                */
/* -------------------------------------------------------------------------- */

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [OPTIONS]\n"
		"\n"
		"Core benchmarks:\n"
		"  --smc N          Pure SMC round-trip (CMD_NOP), N iterations\n"
		"  --smc-axi N      SMC + secure AXI read (CMD_AXI_READ), N iterations\n"
		"  --ns-axi N       Non-secure AXI read via /dev/mem, N iterations\n"
		"  --all N          Run all three core benchmarks with N iterations\n"
		"\n"
		"Extra benchmarks:\n"
		"  --throughput N   Back-to-back CMD_NOP, one timer around all N calls\n"
		"  --session N      Open/close TEE session N times, report stats\n"
		"  --multi-read N   Sweep 1,2,4,8,16 reads per SMC, N iterations each\n"
		"\n"
		"Output:\n"
		"  --csv            Raw per-iteration CSV data (redirect to file)\n"
		"\n"
		"NS switch address: 0x%lx (compile-time, -DNS_SWITCH_ADDR=0x...)\n",
		prog, (unsigned long)NS_SWITCH_ADDR);
}

int main(int argc, char *argv[])
{
	TEEC_Result res;
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_UUID uuid = PTA_BENCHMARK_UUID;
	uint32_t err_origin;
	int do_smc = 0, do_smc_axi = 0, do_ns_axi = 0;
	int do_throughput = 0, do_session = 0, do_multi_read = 0;
	int n_smc = 0, n_smc_axi = 0, n_ns_axi = 0;
	int n_throughput = 0, n_session = 0, n_multi_read = 0;
	int need_tee = 0, sess_open = 0;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--csv") == 0) {
			csv_mode = 1;
		} else if (strcmp(argv[i], "--smc") == 0 && i + 1 < argc) {
			do_smc = 1;
			n_smc = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--smc-axi") == 0 && i + 1 < argc) {
			do_smc_axi = 1;
			n_smc_axi = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--ns-axi") == 0 && i + 1 < argc) {
			do_ns_axi = 1;
			n_ns_axi = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--throughput") == 0 && i + 1 < argc) {
			do_throughput = 1;
			n_throughput = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--session") == 0 && i + 1 < argc) {
			do_session = 1;
			n_session = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--multi-read") == 0 && i + 1 < argc) {
			do_multi_read = 1;
			n_multi_read = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--all") == 0 && i + 1 < argc) {
			int n = atoi(argv[++i]);
			do_smc = do_smc_axi = do_ns_axi = 1;
			n_smc = n_smc_axi = n_ns_axi = n;
		} else {
			usage(argv[0]);
			return 1;
		}
	}

	if (!do_smc && !do_smc_axi && !do_ns_axi &&
	    !do_throughput && !do_session && !do_multi_read) {
		usage(argv[0]);
		return 1;
	}

	need_tee = do_smc || do_smc_axi || do_throughput ||
		   do_session || do_multi_read;

	if (need_tee) {
		res = TEEC_InitializeContext(NULL, &ctx);
		if (res != TEEC_SUCCESS)
			errx(1, "TEEC_InitializeContext failed: 0x%x", res);
	}

	/* Session benchmark opens/closes its own sessions */
	if (do_session)
		bench_session(&ctx, n_session);

	/* Open a persistent session for the remaining TEE benchmarks */
	if (do_smc || do_smc_axi || do_throughput || do_multi_read) {
		res = TEEC_OpenSession(&ctx, &sess, &uuid,
				       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
		if (res != TEEC_SUCCESS)
			errx(1, "TEEC_OpenSession failed: 0x%x origin 0x%x",
			     res, err_origin);
		sess_open = 1;
	}

	if (do_smc)
		bench_smc(&sess, n_smc);

	if (do_throughput)
		bench_throughput(&sess, n_throughput);

	if (do_smc_axi)
		bench_smc_axi(&sess, n_smc_axi);

	if (do_multi_read)
		bench_multi_read(&sess, n_multi_read);

	if (sess_open) {
		TEEC_CloseSession(&sess);
	}

	if (need_tee)
		TEEC_FinalizeContext(&ctx);

	if (do_ns_axi)
		bench_ns_axi(n_ns_axi);

	/* Print derived metrics when running all core benchmarks in text mode */
	if (do_smc && do_smc_axi && do_ns_axi && !csv_mode) {
		printf("\n=== Derived ===\n");
		printf("  s_axi_cost = smc_s_axi_rtt - smc_rtt  (security + AXI cost beyond SMC)\n");
		printf("  SMC overhead = smc_rtt (pure context switch cost)\n");
		printf("  security_overhead = s_axi_rtt - ns_axi_rtt (TrustZone protection cost)\n");
	}

	return 0;
}
