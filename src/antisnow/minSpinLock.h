/*
 * minSpinLock.h
 *
 *  Created on: 2016年10月18日
 *      Author: skyyuan
 */

#ifndef MINSPINLOCK_H_
#define MINSPINLOCK_H_

#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<stdint.h>
#include<string.h>
#include<malloc.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define LIKELY(x)           (__builtin_expect((x),1))
#define UNLIKELY(x)         (__builtin_expect((x),0))
#define UNLIKELY_IF(x)      if (__builtin_expect((x),0))

#define MINLOCK(obj)              spin_lock( &obj, getpid())
#define MINUNLOCK(obj)            spin_unlock( &obj, getpid())

#define nop()						    __asm__ ("pause" )

// return old value
static inline int atomic_comp_swap(volatile void *lockword, int exchange,
		int comperand) {
	__asm__ __volatile__(
			"lock cmpxchg %1, (%2)"
			:"=a"(comperand)
			:"d"(exchange), "r"(lockword), "a"(comperand)
	);
	return comperand;
}

static int SplitString(char* pString, char c, char* pElements[], int n) {
	char *p1, *p2;
	int i = 0;

	for (p1 = p2 = pString; *p1; p1++) {
		if (*p1 == c) {
			*p1 = 0;
			if (i < n)
				pElements[i++] = p2;
			p2 = p1 + 1;
		}
	}
	if (i < n)
		pElements[i++] = p2;
	return i;
}

static inline uint64_t rdtsc() {
	unsigned int lo, hi;
	/* We cannot use "=A", since this would use %rax on x86_64 */
	__asm__ __volatile__ (
			"rdtsc"
			: "=a" (lo), "=d" (hi)
	);
	return (uint64_t) hi << 32 | lo;
}

static inline int Sleep(unsigned int nMilliseconds) {
	struct timespec ts;
	ts.tv_sec = nMilliseconds / 1000;
	ts.tv_nsec = (nMilliseconds % 1000) * 1000000;

	return nanosleep(&ts, NULL);
}

static uint64_t CalcCpuFreq() {
	uint64_t t1;
	uint64_t t2;

	t1 = rdtsc();
	Sleep(100);
	t2 = rdtsc();
	return (t2 - t1) * 10;
}
static uint64_t GetCpuFreq() {
	static uint64_t freq = 0;
	char buf[1024], *p[2];

	if (0 != freq) {
		return freq;
	}

	FILE* fp = fopen("/proc/cpuinfo", "rb");
	if (fp != NULL) {
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			if (2 == SplitString(buf, ':', p, 2)
					&& 0 == strncasecmp(p[0], "cpu MHz", 7)) {
				double f = strtod(p[1], NULL);
				freq = (uint64_t) (f * 1000000.0);
				/*printf("p[1]=%s f=%f freq=%llu\n", p[1], f,freq);*/
				break;
			}
		}
		fclose(fp);
	}
	if (0 == freq) {
		freq = CalcCpuFreq();
	}
	return freq;
}

static inline void spin_lock(volatile int *lock, int id) {
	static unsigned long long ticks_per_second = 0;
	unsigned long long ticks_nop = 0;

	int l;
	int i = 50;
	UNLIKELY_IF(ticks_per_second == 0) {
		ticks_per_second = GetCpuFreq();
	}
	for (l = atomic_comp_swap(lock, id, 0); l != 0 && l != id; l =
			atomic_comp_swap(lock, id, 0)) {
		if (i--) {
			nop();
			++ticks_nop;
		} else {
			if (ticks_nop > ticks_per_second / 50)
				*lock = 0;
			i = 50;
			sched_yield();

			atomic_comp_swap(lock, 0, id);
			break;
		}

	}
}

static inline bool spin_unlock(volatile int *lock, int id) {
	return id == atomic_comp_swap(lock, 0, id);
}

#ifdef	__cplusplus
}
#endif

#endif /* MINSPINLOCK_H_ */
