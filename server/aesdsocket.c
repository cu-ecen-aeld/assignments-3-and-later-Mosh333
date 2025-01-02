#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include "queue.h"  // For SLIST
#include <stdbool.h>
#include <sys/time.h>   // For struct timeval
#include <time.h>       // For struct timespec and time functions
#include "../aesd-char-driver/aesd_ioctl.h"

#define PORT 9000  // the port users will be connecting to
#define BACKLOG 10   // how many pending connections queue will hold

// default to 1 if not specified by Makefile
#ifndef USE_AESD_CHAR_DEVICE
    #define USE_AESD_CHAR_DEVICE 1
#endif

#if USE_AESD_CHAR_DEVICE
    #define FILE_PATH "/dev/aesdchar"
#else
    #define FILE_PATH "/var/tmp/aesdsocketdata"
#endif
#define RECV_BUFFER_SIZE 512
#define SEND_BUFFER_SIZE 512

// char recv_buffer[RECV_BUFFER_SIZE];
// char send_buffer[SEND_BUFFER_SIZE];
int server_sockfd = -1;
// int client_sockfd = -1;
bool program_running = 1;   //flag to communicate program completion

// Mutex for synchronizing file writes
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

// Global declaration for the timestamp thread
pthread_t timestamp_thread;

// Linked list structure for managing threads
struct thread_node_data {
    pthread_t thread_id;
    bool thread_work_completion;
    int client_sockfd;
    SLIST_ENTRY(thread_node_data) conn_node;  // Manage threads with linked list
};

// Declare and initialize the head of the singly linked list
SLIST_HEAD(thread_list, thread_node_data) thread_list_head = SLIST_HEAD_INITIALIZER(thread_list_head);

// Function Prototypes
void free_client_threads_in_list();
void free_resources();
void handle_signal(int signal);
void *process_connection_request(void* thread_arg);
void run_program_in_daemon_mode();
void *timestamp_thread_func();

void free_client_threads_in_list(){
    //loop and close all client threads for cleanup
    struct thread_node_data* thread_data;
    while (!SLIST_EMPTY(&thread_list_head)) {
        thread_data = SLIST_FIRST(&thread_list_head);
        thread_data->thread_work_completion = true;
        pthread_join(thread_data->thread_id, NULL);
        SLIST_REMOVE_HEAD(&thread_list_head, conn_node);
        free(thread_data);
    }
    pthread_mutex_destroy(&file_mutex);
}

void free_resources(){
    // Clean up and close the client socket
    // if (client_sockfd != -1) {
    //     close(client_sockfd);
    //     client_sockfd = -1;
    // }
    free_client_threads_in_list();

    // Clean up and close the server socket
    if (server_sockfd != -1) {
        close(server_sockfd);
        server_sockfd = -1; // Reset value
    }

    // Cleanup and remove the file
#ifndef USE_AESD_CHAR_DEVICE
    // Cleanup and remove the file
    if (remove(FILE_PATH) != 0) {
        syslog(LOG_ERR, "Failed to remove file: %s", strerror(errno));
    }
#endif

    pthread_cancel(timestamp_thread);
    pthread_join(timestamp_thread, NULL);

    // Close the log
    closelog();
}

// Signal Handler code
void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        syslog(LOG_INFO, "Caught signal %d, exiting", signal);

        program_running = false;

        // Shutdown the socket
        if (server_sockfd >= 0) {
            shutdown(server_sockfd, SHUT_RDWR);
        }

        free_resources();
        exit(0);
    }
}

void *process_connection_request(void* thread_arg) {
    struct thread_node_data* tdata = thread_arg;
    int client_sockfd = tdata->client_sockfd;
    char recv_buffer[RECV_BUFFER_SIZE] = {0};
    ssize_t bytes_received = 0;

    // Open the file in append mode
    // printf("Opening file in append mode...\n");
    int local_aesd_fd = open(FILE_PATH, O_CREAT | O_APPEND | O_RDWR, 0644);
    if (local_aesd_fd < 0) {
        syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
        close(client_sockfd);
        return NULL;
    }

    // Receive data from the client
    while ((bytes_received = recv(client_sockfd, recv_buffer, RECV_BUFFER_SIZE - 1, 0)) > 0) {
        recv_buffer[bytes_received] = '\0';  // Null-terminate the received data
        const char *ioc_cmd = "AESDCHAR_IOCSEEKTO:";
        // printf("Received %ld bytes from client: %s\n", bytes_received, recv_buffer);

        // Find newline character
        char *newline = strchr(recv_buffer, '\n');
        pthread_mutex_lock(&file_mutex);

        syslog(LOG_INFO, "Received command: %s", recv_buffer);
        if (strncmp(recv_buffer, ioc_cmd, strlen(ioc_cmd)) == 0) {
            // Handle IOCTL Seek Command
            struct aesd_seekto seekto;
            if (sscanf(recv_buffer + strlen(ioc_cmd), "%d,%d", &seekto.write_cmd, &seekto.write_cmd_offset) == 2) {
                if (ioctl(local_aesd_fd, AESDCHAR_IOCSEEKTO, &seekto) < 0) {
                    syslog(LOG_ERR, "IOCTL failed: %s", strerror(errno));
                } else {
                    syslog(LOG_INFO, "IOCTL Seek Command executed successfully");
                }
            } else {
                syslog(LOG_ERR, "Invalid IOCTL command format: %s", recv_buffer);
            }
        } else {
            if (newline) {
                // Write data up to and including the newline to the file
                if (write(local_aesd_fd, recv_buffer, newline - recv_buffer + 1) < 0) {
                    syslog(LOG_ERR, "Error writing to file: %s", strerror(errno));
                }
                pthread_mutex_unlock(&file_mutex);
                break;  // Exit loop after handling the packet with newline
            } else {
                // Write entire buffer if no newline is found
                if (write(local_aesd_fd, recv_buffer, bytes_received) < 0) {
                    syslog(LOG_ERR, "Error writing to file: %s", strerror(errno));
                }
                pthread_mutex_unlock(&file_mutex);
            }
        }
    }

    if (bytes_received < 0) {
        syslog(LOG_ERR, "Error receiving data: %s", strerror(errno));
    }

    // Send the file contents back to the client
    lseek(local_aesd_fd, 0, SEEK_SET);  // Seek to the start of the file
    while ((bytes_received = read(local_aesd_fd, recv_buffer, RECV_BUFFER_SIZE)) > 0) {
        if (send(client_sockfd, recv_buffer, bytes_received, 0) < 0) {
            syslog(LOG_ERR, "Error sending data: %s", strerror(errno));
            break;
        }
    }

    // Clean up
    close(local_aesd_fd);
    close(client_sockfd);

    tdata->thread_work_completion = true;  // Mark the thread as completed
    return NULL;

}

void *timestamp_thread_func() {

    struct timespec next_timestamp;
    clock_gettime(CLOCK_REALTIME, &next_timestamp);  // Get the current time

    while (program_running) {
        // Set the time for the next timestamp after 10 seconds
        next_timestamp.tv_sec += 10;

        // Sleep until the next 10-second interval
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_timestamp, NULL);

        // Obtain the current time for generating the timestamp
        time_t current_time = time(NULL);
        struct tm *time_struct = localtime(&current_time);
        char formatted_timestamp[128];  // Increased buffer size to be safe

        // Format the timestamp string
        strftime(formatted_timestamp, sizeof(formatted_timestamp), "timestamp:%A, %d-%b-%Y %H:%M:%S %Z\n", time_struct);

        // Synchronize file access with the mutex
        pthread_mutex_lock(&file_mutex);

        // Append timestamp to the log file
        FILE *log_file = fopen(FILE_PATH, "a");
        if (log_file) {
            fputs(formatted_timestamp, log_file);  // Write the formatted timestamp
            fflush(log_file);                      // Force write to disk
            fclose(log_file);
        } else {
            syslog(LOG_ERR, "Unable to open log file for writing timestamp: %s", strerror(errno));
        }

        // Unlock the mutex once file operations are complete
        pthread_mutex_unlock(&file_mutex);
    }

    return NULL;
}

void run_program_in_daemon_mode(){
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "Fork failed: %s", strerror(errno));
        free_resources();
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // Unmask the file mode
    umask(0);

    // Set new session
    if (setsid() < 0) {
        syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
        free_resources();
        exit(EXIT_FAILURE);
    }

    if (chdir("/") < 0) {
        syslog(LOG_ERR, "chdir failed: %s", strerror(errno));
        free_resources();
        exit(EXIT_FAILURE);
    }

    // Close stdin, stdout, and stderr
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Enable syslogging for daemon process
    openlog(NULL, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);
}

int main(int argc, char **argv)
{
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size;
    // char client_ip[INET_ADDRSTRLEN];
    int daemon_mode = 0;

    // Open syslog
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Starting the aesdsocket program");

    // Check for daemon mode
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = 1;
    }

    // Register signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Create socket
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
        return -1;
    }

    // Set socket options
    int yes = 1;
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        syslog(LOG_ERR, "Failed to set SO_REUSEADDR: %s", strerror(errno));
        close(server_sockfd);
        return -1;
    }

    // Bind socket to port 9000
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        syslog(LOG_ERR, "Failed to bind socket: %s", strerror(errno));
        close(server_sockfd);
        return -1;
    }

    // Daemonize if requested
    if (daemon_mode) {
        run_program_in_daemon_mode();
    }

    #ifndef USE_AESD_CHAR_DEVICE
        // printf("Create timestamp!\n");
        // Create the timestamp thread
        if (pthread_create(&timestamp_thread, NULL, timestamp_thread_func, NULL) != 0) {
            syslog(LOG_ERR, "Failed to create timestamp thread: %s", strerror(errno));
            free_resources();
            return -1;
        }
    #endif

    // Listen for incoming connections
    if (listen(server_sockfd, BACKLOG) < 0) {
        syslog(LOG_ERR, "Failed to listen on socket: %s", strerror(errno));
        close(server_sockfd);
        return -1;
    }

    syslog(LOG_INFO, "Server is listening on port %d", PORT);
    // printf("Server is listening on port %d\n", PORT);

    while(program_running){
        // Accept an incoming connection
        // printf("Accepting incoming connection\n");
        client_addr_size = sizeof(client_addr);
        int client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_sockfd < 0) {
            syslog(LOG_ERR, "Failed to accept connection: %s", strerror(errno));
            continue;
        }

        // Log the accepted connection
        // char *client_addr_info = inet_ntoa(client_addr.sin_addr);
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);
        // printf("Server: got connection from %s\n", client_ip);

        // Allocate memory for thread data
        struct thread_node_data *node = malloc(sizeof(struct thread_node_data));
        if (node == NULL) {
            syslog(LOG_ERR, "Failed to allocate memory for thread data");
            close(client_sockfd);
            continue;
        }

        
        node->thread_work_completion = false;
        node->client_sockfd = client_sockfd;
        // Insert the new thread node into the singly linked list
        SLIST_INSERT_HEAD(&thread_list_head, node, conn_node);

        // printf("About to create new thread!\n");
        // Create a new thread to handle the connection
        if (pthread_create(&node->thread_id, NULL, process_connection_request, (void*)node) != 0) {
            syslog(LOG_ERR, "Failed to create thread: %s", strerror(errno));
            close(client_sockfd);
            free(node);
            continue;
        }
        // printf("Created new thread!\n");


    }

    // Cleanup (unreachable due to infinite loop unless something causes an exit out of loop like signal handling, break condition, or program termination)
    free_resources();

    return 0;
}
