// SPDX-License-Identifier: BSD-2-Clause
//
// optee_benchmark -- host-side benchmark for TrustZone overhead on Zynq SoCs
//
// Run on target:
//   optee_benchmark --smc 10000 --csv > smc.csv
//   optee_benchmark --smc-axi 10000 --csv > smc_axi.csv
//   optee_benchmark --ns-axi 10000 --csv > ns_axi.csv

#include "benchmark.h"

int csv_mode;

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [OPTIONS]\n"
		"\n"
		"Benchmarks:\n"
		"  --smc N          Pure SMC round-trip (CMD_NOP), N iterations\n"
		"  --smc-axi N      SMC + secure AXI read (CMD_AXI_READ), N iterations\n"
		"  --ns-axi N       Non-secure AXI read via /dev/mem, N iterations\n"
		"  --all N          Run all three benchmarks with N iterations\n"
		"\n"
		"Output:\n"
		"  --csv            Raw per-iteration CSV data (redirect to file)\n",
		prog);
}

int main(int argc, char *argv[])
{
	TEEC_Result res;
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_UUID uuid = PTA_BENCHMARK_UUID;
	uint32_t err_origin;
	int do_smc = 0, do_smc_axi = 0, do_ns_axi = 0;
	int n_smc = 0, n_smc_axi = 0, n_ns_axi = 0;
	int need_tee, sess_open = 0;
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
		} else if (strcmp(argv[i], "--all") == 0 && i + 1 < argc) {
			int n = atoi(argv[++i]);
			do_smc = do_smc_axi = do_ns_axi = 1;
			n_smc = n_smc_axi = n_ns_axi = n;
		} else {
			usage(argv[0]);
			return 1;
		}
	}

	if (!do_smc && !do_smc_axi && !do_ns_axi) {
		usage(argv[0]);
		return 1;
	}

	need_tee = do_smc || do_smc_axi;

	if (need_tee) {
		res = TEEC_InitializeContext(NULL, &ctx);
		if (res != TEEC_SUCCESS)
			errx(1, "TEEC_InitializeContext failed: 0x%x", res);

		res = TEEC_OpenSession(&ctx, &sess, &uuid,
				       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
		if (res != TEEC_SUCCESS)
			errx(1, "TEEC_OpenSession failed: 0x%x origin 0x%x",
			     res, err_origin);
		sess_open = 1;
	}

	if (do_smc)
		bench_smc(&sess, n_smc);
	if (do_smc_axi)
		bench_smc_axi(&sess, n_smc_axi);

	if (sess_open)
		TEEC_CloseSession(&sess);
	if (need_tee)
		TEEC_FinalizeContext(&ctx);

	if (do_ns_axi)
		bench_ns_axi(n_ns_axi);

	return 0;
}
