#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "common/utility.h"
#include "tssx/selective.h"
#include "tssx/string-set.h"

StringSet selective_set = SS_INITIALIZER;

int server_check_use_tssx(int socket_fd) {
	int return_code;

	// Haven't checked this yet for the server's sockets
	if ((return_code = is_domain_and_stream_socket(socket_fd)) != 1) {
		// Either error or false (the socket is not a domain socket)
		return return_code;
	}

	if (!ss_is_initialized(&selective_set)) {
		initialize_selective_set();
	}

	// Empty means all sockets should use TSSX
	return in_selective_set(socket_fd);
}

int client_check_use_tssx(int socket_fd, const struct sockaddr* address) {
	struct sockaddr_un* domain_address;

	if (!ss_is_initialized(&selective_set)) {
		initialize_selective_set();
	}

	domain_address = (struct sockaddr_un*)address;

	if (ss_contains(&selective_set, domain_address->sun_path)) return true;

	return false;
}

void initialize_selective_set() {
	const char* variable;
	if ((variable = fetch_tssx_variable()) != NULL) {
		parse_tssx_variable(variable);
	}

#ifdef DEBUG
	if (variable == NULL) {
		fprintf(stderr, "Enabling TSSX for \033[92mall\033[0m sockets ...\n");
	}
#endif
}

const char* fetch_tssx_variable() {
	return getenv("USE_TSSX");
}

void parse_tssx_variable(const char* variable) {
	char* buffer = (char*)malloc(strlen(variable));
	strcpy(buffer, variable);

	const char* path;
	for (path = strtok(buffer, " "); path; path = strtok(NULL, " ")) {
		bool b = ss_insert(&selective_set, path);
		assert(b);
		(void) b;
	}

	free(buffer);
}

int in_selective_set(int socket_fd) {
	struct sockaddr_un address;
	socklen_t length = sizeof address;

	if (getsockname(socket_fd, (struct sockaddr*)&address, &length) == -1) {
		fprintf(stderr, "Error getting socket path");
		return ERROR;
	}

// address.sun_path[length - sizeof(address.sun_family) - 1] = '\0';

#ifdef DEBUG
	if (ss_contains(&selective_set, address.sun_path)) {
		fprintf(stderr, "Enabling TSSX for '%s' ...\n", address.sun_path);
	} else {
		fprintf(stderr, "Disabling TSSX for '%s' ...\n", address.sun_path);
	}
#endif

	return ss_contains(&selective_set, address.sun_path);
}

int is_domain_and_stream_socket(int socket_fd) {
	return is_domain_socket(socket_fd) && is_stream_socket(socket_fd);
}

int is_domain_socket(int socket_fd) {
	// Doesn't work on OS X (no SO_DOMAIN in sys/socket.h)
	// return get_socket_option(socket_fd, SO_DOMAIN) == AF_LOCAL;
	struct sockaddr address;
	socklen_t length = sizeof address;

	if (getsockname(socket_fd, &address, &length) == -1) {
		fprintf(stderr, "Error getting socket family");
		return ERROR;
	}

	// AF_LOCAL is an alias for AF_UNIX (i.e. they have the same value)
	return address.sa_family == AF_LOCAL;
}

int is_stream_socket(int socket_fd) {
	int option = get_socket_option(socket_fd, SO_TYPE);
	return option == ERROR ? ERROR : (option == SOCK_STREAM);
}

int get_socket_option(int socket_fd, int option_name) {
	int return_code;
	socklen_t option;
	socklen_t option_length = sizeof option;

	// clang-format off
	return_code = getsockopt(
    socket_fd,
    SOL_SOCKET,
    option_name,
    &option,
    &option_length
  );
	// clang-format on

	if (return_code == -1) {
		fprintf(stderr, "Error getting socket option");
		return ERROR;
	}

	return option;
}