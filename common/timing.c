#include <stddef.h>
#include <sys/time.h>
#include "timing.h"

static unsigned long start_time;

unsigned long micros() {
	struct timeval current_time;
	gettimeofday(&current_time, NULL);
	return current_time.tv_sec * (int)1E6 + current_time.tv_usec;
}

void tic() {
	start_time = micros();
}

unsigned long toc() {
	return (micros() - start_time);
}
