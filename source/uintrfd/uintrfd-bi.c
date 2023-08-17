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
#define __NR_uintr_register_handler	449
#define __NR_uintr_unregister_handler	450
#define __NR_uintr_create_fd		451
#define __NR_uintr_register_sender	452
#define __NR_uintr_unregister_sender	453
#define __NR_uintr_wait			454
#endif

#define uintr_register_handler(handler, flags)	syscall(__NR_uintr_register_handler, handler, flags)
#define uintr_unregister_handler(flags)		syscall(__NR_uintr_unregister_handler, flags)
#define uintr_create_fd(vector, flags)		syscall(__NR_uintr_create_fd, vector, flags)
#define uintr_register_sender(fd, flags)	syscall(__NR_uintr_register_sender, fd, flags)
#define uintr_unregister_sender(fd, flags)	syscall(__NR_uintr_unregister_sender, fd, flags)
#define uintr_wait(flags)			syscall(__NR_uintr_wait, flags)

#define SERVER_TOKEN 0
#define CLIENT_TOKEN 1

volatile unsigned long uintr_received[2];
int uintrfd_client;
int uintrfd_server;
int uipi_index[2];

void __attribute__ ((interrupt))
     __attribute__((target("general-regs-only", "inline-all-stringops")))
     ui_handler(struct __uintr_frame *ui_frame,
		unsigned long long vector) {

		// The vector number is same as the token
		uintr_received[vector] = 1;
}

int setup_handler_with_vector(int vector) {
	int fd;

	if (uintr_register_handler(ui_handler, 0))
		throw("Interrupt handler register error\n");

	// Create a new uintrfd object and get the corresponding
	// file descriptor.
	fd = uintr_create_fd(vector, 0);

	if (fd < 0)
		throw("Interrupt vector registration error\n");

	return fd;
}

void uintrfd_wait(unsigned int token) {

	// Keep spinning until the interrupt is received
	while (!uintr_received[token]);

	uintr_received[token] = 0;
}

void uintrfd_notify(unsigned int token) {
	_senduipi(uipi_index[token]);
}

void setup_client(void) {

	uintrfd_client = setup_handler_with_vector(CLIENT_TOKEN);

	// Wait for client to setup its FD.
	while (!uintrfd_server)
		usleep(10);

	uipi_index[SERVER_TOKEN] = uintr_register_sender(uintrfd_server, 0);
	if (uipi_index[SERVER_TOKEN] < 0)
		throw("Sender register error\n");

	// Enable interrupts
	_stui();

}

void setup_server(void) {

	uintrfd_server = setup_handler_with_vector(SERVER_TOKEN);

	// Wait for client to setup its FD.
	while (!uintrfd_client)
		usleep(10);

	uipi_index[CLIENT_TOKEN] = uintr_register_sender(uintrfd_client, 0);

	// Enable interrupts
	_stui();

}

void *client_communicate(void *arg) {

	struct Arguments* args = (struct Arguments*)arg;
	int loop;

	setup_client();

	for (loop = args->count; loop > 0; --loop) {
		uintrfd_wait(CLIENT_TOKEN);
		uintrfd_notify(SERVER_TOKEN);
	}

	return NULL;
}

void server_communicate(struct Arguments* args) {

	struct Benchmarks bench;
	int message;

	setup_benchmarks(&bench);

	setup_server();

	for (message = 0; message < args->count; ++message) {
		bench.single_start = now();
		uintrfd_notify(CLIENT_TOKEN);
		uintrfd_wait(SERVER_TOKEN);
		benchmark(&bench);
	}

	// The message size is always one (it's just a signal)
	args->size = 1;
	evaluate(&bench, args);
}

void communicate(struct Arguments* args) {

	pthread_t pt;

	// Create another thread
	if (pthread_create(&pt, NULL, &client_communicate, args)) {
		throw("Error creating sender thread");
	}

	server_communicate(args);
}

int main(int argc, char* argv[]) {

	struct Arguments args;

	parse_arguments(&args, argc, argv);

	communicate(&args);

	return EXIT_SUCCESS;
}
