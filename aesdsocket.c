#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

static volatile sig_atomic_t stop_requested = 0;

static void signal_handler(int signo)
{
    (void)signo;
    stop_requested = 1;
}

static int send_file_to_client(int client_fd)
{
    FILE *file;
    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    file = fopen(DATA_FILE, "r");
    if (file == NULL) {
        perror("fopen read");
        return -1;
    }

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (send(client_fd, buffer, bytes_read, 0) == -1) {
            perror("send");
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 0;
}

static int receive_packet_and_save(int client_fd)
{
    char buffer[BUFFER_SIZE];
    char *packet = NULL;
    size_t packet_size = 0;
    ssize_t bytes_received;
    bool newline_found = false;

    while (!newline_found) {
        bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes_received == 0) {
    		break;
	}

	if (bytes_received < 0) {
    		free(packet);
    		return -1;
	}

        size_t bytes_to_copy = bytes_received;

        for (ssize_t i = 0; i < bytes_received; i++) {
            if (buffer[i] == '\n') {
                bytes_to_copy = i + 1;
                newline_found = true;
                break;
            }
        }

        char *new_packet = realloc(packet, packet_size + bytes_to_copy);
        if (new_packet == NULL) {
            free(packet);
            return -1;
        }

        packet = new_packet;
        memcpy(packet + packet_size, buffer, bytes_to_copy);
        packet_size += bytes_to_copy;
    }
	if (packet_size == 0) {
        free(packet);
        return -1;
    }
    if (!newline_found) {
        char *new_packet = realloc(packet, packet_size + 1);
        if (new_packet == NULL) {
            free(packet);
            return -1;
        }

        packet = new_packet;
        packet[packet_size] = '\n';
        packet_size++;
    }
    FILE *file = fopen(DATA_FILE, "a");
    if (file == NULL) {
        free(packet);
        perror("fopen append");
        return -1;
    }

    fwrite(packet, 1, packet_size, file);
    fclose(file);
    free(packet);

    return 0;
}

int main(int argc, char *argv[])
{
    int server_fd;
    int client_fd;
    int opt = 1;
    bool daemon_mode = false;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    char client_ip[INET_ADDRSTRLEN];

    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = true;
    }

    openlog(NULL, 0, LOG_USER);

    	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = signal_handler;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return -1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(server_fd);
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_fd);
        return -1;
    }

    if (daemon_mode) {
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            close(server_fd);
            return -1;
        }

        if (pid > 0) {
            close(server_fd);
            return 0;
        }

        if (setsid() == -1) {
            perror("setsid");
            close(server_fd);
            return -1;
        }

        chdir("/");
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    if (listen(server_fd, 10) == -1) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    while (!stop_requested) {
        client_len = sizeof(client_addr);

        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            if (errno == EINTR && stop_requested) {
                break;
            }

            perror("accept");
            continue;
        }

        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        syslog(LOG_DEBUG, "Accepted connection from %s", client_ip);

        if (receive_packet_and_save(client_fd) == 0) {
            send_file_to_client(client_fd);
        }

        close(client_fd);
        syslog(LOG_DEBUG, "Closed connection from %s", client_ip);
    }

    syslog(LOG_DEBUG, "Caught signal, exiting");

    close(server_fd);
    remove(DATA_FILE);
    closelog();

    return 0;
}
