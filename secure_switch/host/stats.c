// SPDX-License-Identifier: BSD-2-Clause
// Statistics computation and printing helpers

#include "benchmark.h"

uint64_t timespec_to_ns(struct timespec *ts)
{
	return (uint64_t)ts->tv_sec * 1000000000ULL + ts->tv_nsec;
}

struct stats compute_stats_u64(const uint64_t *data, int n)
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

// Convenience wrapper: widens uint32_t array to uint64_t before computing
struct stats compute_stats_u32(const uint32_t *data, int n)
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

void print_stats(const char *unit, struct stats *s)
{
	printf("  avg: %.1f %s    stddev: %.1f %s\n", s->avg, unit, s->stddev, unit);
	printf("  min: %lu %s     max: %lu %s\n",
	       (unsigned long)s->min, unit, (unsigned long)s->max, unit);
}
