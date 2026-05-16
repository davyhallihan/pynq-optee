#ifndef BENCHMARK_H
#define BENCHMARK_H

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

#ifndef NS_SWITCH_ADDR
#define NS_SWITCH_ADDR 0x40000000  /* PYNQ-Z2 default */
#endif

#define NS_SWITCH_SIZE 0x1000

struct stats {
	uint64_t min;
	uint64_t max;
	double avg;
	double stddev;
};

extern int csv_mode;

uint64_t timespec_to_ns(struct timespec *ts);
struct stats compute_stats_u64(const uint64_t *data, int n);
struct stats compute_stats_u32(const uint32_t *data, int n);
void print_stats(const char *unit, struct stats *s);

void bench_smc(TEEC_Session *sess, int n);
void bench_smc_axi(TEEC_Session *sess, int n);
void bench_ns_axi(int n);

#endif /* BENCHMARK_H */
