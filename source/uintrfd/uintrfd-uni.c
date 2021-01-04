#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <x86gprintrin.h>

#define __USE_GNU
#include <pthread.h>
#include <sched.h>

#include "common/common.h"

#ifndef __NR_uintr_register_handler
#define __NR_uintr_register_handler	471
#define __NR_uintr_unregister_handler	472
#define __NR_uintr_create_fd		473
#define __NR_uintr_register_sender	474
#define __NR_uintr_unregister_sender	475
#define __NR_uintr_wait			476
#endif

#define uintr_register_handler(handler, flags)	syscall(__NR_uintr_register_handler, handler, flags)
#define uintr_unregister_handler(flags)		syscall(__NR_uintr_unregister_handler, flags)
#define uintr_create_fd(vector, flags)		syscall(__NR_uintr_create_fd, vector, flags)
#define uintr_register_sender(fd, flags)	syscall(__NR_uintr_register_sender, fd, flags)
#define uintr_unregister_sender(ipi_idx, flags)	syscall(__NR_uintr_unregister_sender, ipi_idx, flags)
#define uintr_wait(flags)			syscall(__NR_uintr_wait, flags)

volatile unsigned long uintr_received;
volatile unsigned int uintr_count = 0;
int descriptor;

struct Benchmarks bench;

void __attribute__ ((interrupt))
     __attribute__((target("general-regs-only", "inline-all-stringops")))
     ui_handler(struct __uintr_frame *ui_frame,
		unsigned long long vector) {

	benchmark(&bench);
	uintr_count++;
	uintr_received = 1;
}

void *client_communicate(void *arg) {

	struct Arguments* args = (struct Arguments*)arg;
	int loop;

	int uipi_index = uintr_register_sender(descriptor, 0);
	if (uipi_index < 0)
		throw("Sender register error\n");

	for (loop = args->count; loop > 0; --loop) {

		uintr_received = 0;
		bench.single_start = now();

		// Send User IPI
		_senduipi(uipi_index);

		while (!uintr_received){
			// Keep spinning until this user interrupt is received.
		}
	}

	return NULL;
}

void server_communicate(int descriptor, struct Arguments* args) {

	while (uintr_count < args->count) {
		//Keep spinning until all user interrupts are delivered.
	}

	// The message size is always one (it's just a signal)
	args->size = 1;
	evaluate(&bench, args);
}

void communicate(int descriptor, struct Arguments* args) {

	pthread_t pt;

	setup_benchmarks(&bench);

	// Create another thread
	if (pthread_create(&pt, NULL, &client_communicate, args)) {
		throw("Error creating sender thread");
	}

	server_communicate(descriptor, args);

	close(descriptor);
}

int main(int argc, char* argv[]) {

	struct Arguments args;

	if (uintr_register_handler(ui_handler, 0))
		throw("Interrupt handler register error\n");

	// Create a new uintrfd object and get the corresponding
	// file descriptor.
	descriptor = uintr_create_fd(0, 0);
	if (descriptor < 0)
		throw("Interrupt vector allocation error\n");

	// Enable interrupts
	_stui();

	parse_arguments(&args, argc, argv);

	communicate(descriptor, &args);

	return EXIT_SUCCESS;
}
