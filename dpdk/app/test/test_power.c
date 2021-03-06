/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 * 
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include <cmdline_parse.h>

#include "test.h"

#ifdef RTE_LIBRTE_POWER

#include <rte_power.h>

#define TEST_POWER_LCORE_ID      2U
#define TEST_POWER_LCORE_INVALID ((unsigned)RTE_MAX_LCORE)
#define TEST_POWER_FREQS_NUM_MAX ((unsigned)RTE_MAX_LCORE_FREQS)

#define TEST_POWER_SYSFILE_CUR_FREQ \
	"/sys/devices/system/cpu/cpu%u/cpufreq/scaling_cur_freq"

static uint32_t total_freq_num;
static uint32_t freqs[TEST_POWER_FREQS_NUM_MAX];

static int
check_cur_freq(unsigned lcore_id, uint32_t idx)
{
#define TEST_POWER_CONVERT_TO_DECIMAL 10
	FILE *f;
	char fullpath[PATH_MAX];
	char buf[BUFSIZ];
	uint32_t cur_freq;
	int ret = -1;

	if (rte_snprintf(fullpath, sizeof(fullpath),
		TEST_POWER_SYSFILE_CUR_FREQ, lcore_id) < 0) {
		return 0;
	}
	f = fopen(fullpath, "r");
	if (f == NULL) {
		return 0;
	}
	if (fgets(buf, sizeof(buf), f) == NULL) {
		goto fail_get_cur_freq;
	}
	cur_freq = strtoul(buf, NULL, TEST_POWER_CONVERT_TO_DECIMAL);
	ret = (freqs[idx] == cur_freq ? 0 : -1);

fail_get_cur_freq:
	fclose(f);

	return ret;
}

/* Check rte_power_freqs() */
static int
check_power_freqs(void)
{
	uint32_t ret;

	total_freq_num = 0;
	memset(freqs, 0, sizeof(freqs));

	/* test with an invalid lcore id */
	ret = rte_power_freqs(TEST_POWER_LCORE_INVALID, freqs,
					TEST_POWER_FREQS_NUM_MAX);
	if (ret > 0) {
		printf("Unexpectedly get available freqs successfully on "
				"lcore %u\n", TEST_POWER_LCORE_INVALID);
		return -1;
	}

	/* test with NULL buffer to save available freqs */
	ret = rte_power_freqs(TEST_POWER_LCORE_ID, NULL,
				TEST_POWER_FREQS_NUM_MAX);
	if (ret > 0) {
		printf("Unexpectedly get available freqs successfully with "
			"NULL buffer on lcore %u\n", TEST_POWER_LCORE_ID);
		return -1;
	}

	/* test of getting zero number of freqs */
	ret = rte_power_freqs(TEST_POWER_LCORE_ID, freqs, 0);
	if (ret > 0) {
		printf("Unexpectedly get available freqs successfully with "
			"zero buffer size on lcore %u\n", TEST_POWER_LCORE_ID);
		return -1;
	}

	/* test with all valid input parameters */
	ret = rte_power_freqs(TEST_POWER_LCORE_ID, freqs,
				TEST_POWER_FREQS_NUM_MAX);
	if (ret == 0 || ret > TEST_POWER_FREQS_NUM_MAX) {
		printf("Fail to get available freqs on lcore %u\n",
						TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Save the total number of available freqs */
	total_freq_num = ret;

	return 0;
}

/* Check rte_power_get_freq() */
static int
check_power_get_freq(void)
{
	int ret;
	uint32_t count;

	/* test with an invalid lcore id */
	count = rte_power_get_freq(TEST_POWER_LCORE_INVALID);
	if (count < TEST_POWER_FREQS_NUM_MAX) {
		printf("Unexpectedly get freq index successfully on "
				"lcore %u\n", TEST_POWER_LCORE_INVALID);
		return -1;
	}

	count = rte_power_get_freq(TEST_POWER_LCORE_ID);
	if (count >= TEST_POWER_FREQS_NUM_MAX) {
		printf("Fail to get the freq index on lcore %u\n",
						TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Check the current frequency */
	ret = check_cur_freq(TEST_POWER_LCORE_ID, count);
	if (ret < 0)
		return -1;

	return 0;
}

/* Check rte_power_set_freq() */
static int
check_power_set_freq(void)
{
	int ret;

	/* test with an invalid lcore id */
	ret = rte_power_set_freq(TEST_POWER_LCORE_INVALID, 0);
	if (ret >= 0) {
		printf("Unexpectedly set freq index successfully on "
				"lcore %u\n", TEST_POWER_LCORE_INVALID);
		return -1;
	}

	/* test with an invalid freq index */
	ret = rte_power_set_freq(TEST_POWER_LCORE_ID,
				TEST_POWER_FREQS_NUM_MAX);
	if (ret >= 0) {
		printf("Unexpectedly set an invalid freq index (%u)"
			"successfully on lcore %u\n", TEST_POWER_FREQS_NUM_MAX,
							TEST_POWER_LCORE_ID);
		return -1;
	}

	/**
	 * test with an invalid freq index which is right one bigger than
	 * total number of freqs
	 */
	ret = rte_power_set_freq(TEST_POWER_LCORE_ID, total_freq_num);
	if (ret >= 0) {
		printf("Unexpectedly set an invalid freq index (%u)"
			"successfully on lcore %u\n", total_freq_num,
						TEST_POWER_LCORE_ID);
		return -1;
	}
	ret = rte_power_set_freq(TEST_POWER_LCORE_ID, total_freq_num - 1);
	if (ret < 0) {
		printf("Fail to set freq index on lcore %u\n",
					TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Check the current frequency */
	ret = check_cur_freq(TEST_POWER_LCORE_ID, total_freq_num - 1);
	if (ret < 0)
		return -1;

	return 0;
}

/* Check rte_power_freq_down() */
static int
check_power_freq_down(void)
{
	int ret;

	/* test with an invalid lcore id */
	ret = rte_power_freq_down(TEST_POWER_LCORE_INVALID);
	if (ret >= 0) {
		printf("Unexpectedly scale down successfully the freq on "
				"lcore %u\n", TEST_POWER_LCORE_INVALID);
		return -1;
	}

	/* Scale down to min and then scale down one step */
	ret = rte_power_freq_min(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale down the freq to min on lcore %u\n",
							TEST_POWER_LCORE_ID);
		return -1;
	}
	ret = rte_power_freq_down(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale down the freq on lcore %u\n",
						TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Check the current frequency */
	ret = check_cur_freq(TEST_POWER_LCORE_ID, total_freq_num - 1);
	if (ret < 0)
		return -1;

	/* Scale up to max and then scale down one step */
	ret = rte_power_freq_max(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale up the freq to max on lcore %u\n",
							TEST_POWER_LCORE_ID);
		return -1;
	}
	ret = rte_power_freq_down(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf ("Fail to scale down the freq on lcore %u\n",
						TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Check the current frequency */
	ret = check_cur_freq(TEST_POWER_LCORE_ID, 1);
	if (ret < 0)
		return -1;

	return 0;
}

/* Check rte_power_freq_up() */
static int
check_power_freq_up(void)
{
	int ret;

	/* test with an invalid lcore id */
	ret = rte_power_freq_up(TEST_POWER_LCORE_INVALID);
	if (ret >= 0) {
		printf("Unexpectedly scale up successfully the freq on %u\n",
						TEST_POWER_LCORE_INVALID);
		return -1;
	}

	/* Scale down to min and then scale up one step */
	ret = rte_power_freq_min(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale down the freq to min on lcore %u\n",
							TEST_POWER_LCORE_ID);
		return -1;
	}
	ret = rte_power_freq_up(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale up the freq on lcore %u\n",
						TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Check the current frequency */
	ret = check_cur_freq(TEST_POWER_LCORE_ID, total_freq_num - 2);
	if (ret < 0)
		return -1;

	/* Scale up to max and then scale up one step */
	ret = rte_power_freq_max(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale up the freq to max on lcore %u\n",
						TEST_POWER_LCORE_ID);
		return -1;
	}
	ret = rte_power_freq_up(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale up the freq on lcore %u\n",
						TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Check the current frequency */
	ret = check_cur_freq(TEST_POWER_LCORE_ID, 0);
	if (ret < 0)
		return -1;

	return 0;
}

/* Check rte_power_freq_max() */
static int
check_power_freq_max(void)
{
	int ret;

	/* test with an invalid lcore id */
	ret = rte_power_freq_max(TEST_POWER_LCORE_INVALID);
	if (ret >= 0) {
		printf("Unexpectedly scale up successfully the freq to max on "
				"lcore %u\n", TEST_POWER_LCORE_INVALID);
		return -1;
	}
	ret = rte_power_freq_max(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale up the freq to max on lcore %u\n",
						TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Check the current frequency */
	ret = check_cur_freq(TEST_POWER_LCORE_ID, 0);
	if (ret < 0)
		return -1;

	return 0;
}

/* Check rte_power_freq_min() */
static int
check_power_freq_min(void)
{
	int ret;

	/* test with an invalid lcore id */
	ret = rte_power_freq_min(TEST_POWER_LCORE_INVALID);
	if (ret >= 0) {
		printf("Unexpectedly scale down successfully the freq to min "
				"on lcore %u\n", TEST_POWER_LCORE_INVALID);
		return -1;
	}
	ret = rte_power_freq_min(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Fail to scale down the freq to min on lcore %u\n",
							TEST_POWER_LCORE_ID);
		return -1;
	}

	/* Check the current frequency */
	ret = check_cur_freq(TEST_POWER_LCORE_ID, total_freq_num - 1);
	if (ret < 0)
		return -1;

	return 0;
}

int
test_power(void)
{
	int ret = -1;

	/* test of init power management for an invalid lcore */
	ret = rte_power_init(TEST_POWER_LCORE_INVALID);
	if (ret == 0) {
		printf("Unexpectedly initialise power management successfully "
				"for lcore %u\n", TEST_POWER_LCORE_INVALID);
		return -1;
	}

	ret = rte_power_init(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Cannot initialise power management for lcore %u\n",
							TEST_POWER_LCORE_ID);
		return -1;
	}

	/**
	 * test of initialising power management for the lcore which has
	 * been initialised
	 */
	ret = rte_power_init(TEST_POWER_LCORE_ID);
	if (ret == 0) {
		printf("Unexpectedly init successfully power twice on "
					"lcore %u\n", TEST_POWER_LCORE_ID);
		return -1;
	}

	ret = check_power_freqs();
	if (ret < 0)
		goto fail_all;

	if (total_freq_num < 2) {
		rte_power_exit(TEST_POWER_LCORE_ID);
		printf("Frequency can not be changed due to CPU itself\n");
		return 0;
	}

	ret = check_power_get_freq();
	if (ret < 0)
		goto fail_all;

	ret = check_power_set_freq();
	if (ret < 0)
		goto fail_all;

	ret = check_power_freq_down();
	if (ret < 0)
		goto fail_all;

	ret = check_power_freq_up();
	if (ret < 0)
		goto fail_all;

	ret = check_power_freq_max();
	if (ret < 0)
		goto fail_all;

	ret = check_power_freq_min();
	if (ret < 0)
		goto fail_all;

	ret = rte_power_exit(TEST_POWER_LCORE_ID);
	if (ret < 0) {
		printf("Cannot exit power management for lcore %u\n",
						TEST_POWER_LCORE_ID);
		return -1;
	}

	/**
	 * test of exiting power management for the lcore which has been exited
	 */
	ret = rte_power_exit(TEST_POWER_LCORE_ID);
	if (ret == 0) {
		printf("Unexpectedly exit successfully power management twice "
					"on lcore %u\n", TEST_POWER_LCORE_ID);
		return -1;
	}

	/* test of exit power management for an invalid lcore */
	ret = rte_power_exit(TEST_POWER_LCORE_INVALID);
	if (ret == 0) {
		printf("Unpectedly exit power management successfully for "
				"lcore %u\n", TEST_POWER_LCORE_INVALID);
		return -1;
	}

	return 0;

fail_all:
	rte_power_exit(TEST_POWER_LCORE_ID);

	return -1;
}

#else /* RTE_LIBRTE_POWER */

int
test_power(void)
{
	printf("The power library is not included in this build\n");
	return 0;
}

#endif /* RTE_LIBRTE_POWER */

