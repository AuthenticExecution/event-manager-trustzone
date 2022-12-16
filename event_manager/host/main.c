#include <stdio.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "logging.h"
#include "networking.h"

// TODO port as cmd arg
#define PORT 1236

// TODO check arguments of functions and variables

int main(int argc, char const* argv[])
{
	int server_fd, client_socket;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);

	// Creating socket file descriptor
	if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		ERROR("socket failed");
		exit(EXIT_FAILURE);
	}

	// Forcefully attaching socket to the port 8080
	if(setsockopt(
        server_fd, SOL_SOCKET,
		SO_REUSEADDR | SO_REUSEPORT, 
        &opt,
		sizeof(opt)
    )) {
		ERROR("setsockopt failed");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);

	// Forcefully attaching socket to the port 8080
	if(bind(
        server_fd, 
        (struct sockaddr*)&address,
		sizeof(address)
        ) < 0
    ) {
		ERROR("bind failed");
		exit(EXIT_FAILURE);
	}

    // listen to port 8080
    //TODO check this 3
	if(listen(server_fd, 3) < 0) {
		ERROR("listen failed");
		exit(EXIT_FAILURE);
	}

    while(1) {
        DEBUG("Waiting for new connection");
        
        if((client_socket = accept(
            server_fd,
            (struct sockaddr*)&address,
            (socklen_t*)&addrlen
            )) < 0
        ) {
            WARNING("failed to accept new connection");
            exit(EXIT_FAILURE);
        }

        INFO("Accepted new connection");

        // TODO call event manager
        CommandMessage m = read_command_message(client_socket);
        if(m == NULL) {
            ERROR("Failed to read command");
        } else {
            INFO("Read cmd. ID: %d buf size: %lu", m->code, m->message->size);
            destroy_command_message(m);
        }

        // closing the connected socket
        close(client_socket);
        DEBUG("Connection closed");
    }

	// closing the listening socket
	shutdown(server_fd, SHUT_RDWR);
	return 0;
}
