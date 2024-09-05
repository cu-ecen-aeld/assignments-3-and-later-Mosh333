/*
** aesdsocket.c -- a stream socket server adapted from https://beej.us/guide/bgnet/html/#a-simple-stream-server
*/


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
#include <sys/syslog.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PORT 9000  // the port users will be connecting to
#define BACKLOG 10   // how many pending connections queue will hold
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define RECV_BUFFER_SIZE 512
#define SEND_BUFFER_SIZE 512
char recv_buffer[RECV_BUFFER_SIZE];
char send_buffer[SEND_BUFFER_SIZE];
FILE *aesd_file = NULL;
int server_sockfd = -1;
int client_sockfd = -1;

// Function Prototypes
void free_resources();
void handle_signal(int signal);
int process_connection_request(int client_sockfd);
void run_program_in_daemon_mode();


int main(int argc, char **argv)
{
    // int client_sockfd;
    // See Lecture video on Sockets @ 7:40 that says either approach is okay for the assignment
    struct sockaddr_in server_addr, client_addr; //going with simpler, clasic approach, not the new struct addrinfo serv_addr approach
    socklen_t client_addr_size;
    char client_ip[INET_ADDRSTRLEN];
    int daemon_mode = -1;

    // Open syslog
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Starting the aesdsocket program");

    // 5
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = 1;
    }


    // 2h Register signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // 2b Create socket
    server_sockfd = socket(AF_INET,SOCK_STREAM,0);

    if (server_sockfd < 0) {
        syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
        return -1;
    }

    // Set the socket option to reuse the address
    int yes=1;
    if (setsockopt(server_sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) < 0) {
        syslog(LOG_ERR, "Failed to set SO_REUSEADDR: %s", strerror(errno));
        close(server_sockfd);
        return -1;
    }

    // 2b Bind socket to port 9000
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Bind to any available ip address
    server_addr.sin_port = htons(PORT); // Use port 9000

    if (bind(server_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        syslog(LOG_ERR, "Failed to bind socket: %s", strerror(errno));
        close(server_sockfd);
        return -1;
    }

    printf("Socket created and bound to port %d\n", PORT);
    syslog(LOG_INFO, "Socket created and bound to port %d", PORT);

    // 5
    if (daemon_mode==1) {
        run_program_in_daemon_mode(argv[0]);
    }

    // 2c Listen for incoming connections
    if (listen(server_sockfd, BACKLOG) < 0) {
        syslog(LOG_ERR, "Failed to listen on socket: %s", strerror(errno));
        close(server_sockfd);
        return -1;
    }

    syslog(LOG_INFO, "Server is listening on port %d", PORT);

    while(1){
        // 2c Accept an incoming connection
        client_addr_size = sizeof(client_addr);
        client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_sockfd < 0) {
            syslog(LOG_ERR, "Failed to accept connection: %s", strerror(errno));
            close(server_sockfd);
            return -1;
        }

        // 2d Log the accepted connection
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        // 2e and 2f code below
        process_connection_request(client_sockfd);


        // 2g Log the closed connection
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }
    // Cleanup
    free_resources();

    return 0;
}

void free_resources(){
    // Clean up and close the client socket
    if (client_sockfd != -1) {
        close(client_sockfd);
        client_sockfd = -1;
    }

    // Clean up and close the server socket
    if (server_sockfd != -1) {
        close(server_sockfd);
        server_sockfd = -1; // Reset value
    }

    // Cleanup remove the file
    if (remove(FILE_PATH) != 0) {
        syslog(LOG_ERR, "Failed to remove file: %s", strerror(errno));
    }

    // Unset global pointer
    if (aesd_file) {
        // doing fclose(aesd_file);  will lead to 
        aesd_file = NULL; // Reset value
    }

    // Close the log
    closelog();
}

// 2h code
void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        syslog(LOG_INFO, "Caught signal %d, exiting", signal);


        // Shutdown the socket
        if (server_sockfd >= 0) {
            shutdown(server_sockfd, SHUT_RDWR);
        }

        free_resources();

        exit(0);
    }   

}

// refactoring out 2e&2f code so it's easier to read
int process_connection_request(int client_sockfd) {
    // Step 2e: Receive data from the client and append to a file
    
    // fopen() in append mode with open a file if it does not exist
    aesd_file = fopen(FILE_PATH, "a");
    if (!aesd_file) {
        syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
        close(client_sockfd);
        return -1;
    }

    // https://www.tutorialspoint.com/c_standard_library
    // long as we can handle messages of size:
    // https://github.com/cu-ecen-aeld/assignment-autotest/blob/master/test/assignment5/long_string.txt
    // we should be okay
    int bytes_received;
    
    while ((bytes_received = recv(client_sockfd, recv_buffer, RECV_BUFFER_SIZE - 1, 0)) > 0) {
        recv_buffer[bytes_received] = '\0';  // Null-terminate the received data

        // Find newline character to identify a packet
        char *newline_pos = strchr(recv_buffer, '\n');
        if (newline_pos != NULL) {
            *newline_pos = '\0';  // Replace newline with null terminator for string

            // Append the received data (up to the newline) to file
            fputs(recv_buffer, aesd_file);  //Delimited by newline
            fputs("\n", aesd_file);  // Add the newline back in the file
            fflush(aesd_file);       // Force the fputted data to be written disk

            // Close the file after writing
            fclose(aesd_file);
            // 2f, packet process completion, return full content of aesd_file
            aesd_file = fopen(FILE_PATH, "r");
            if (!aesd_file) {
                syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
                close(client_sockfd);
                return -1;
            }

            // Read the file content and send it back to the client
            size_t bytes_read;
            while ((bytes_read = fread(send_buffer, 1, SEND_BUFFER_SIZE, aesd_file)) > 0) {
                if (send(client_sockfd, send_buffer, bytes_read, 0) == -1) {
                    syslog(LOG_ERR, "Failed to send file content to client: %s", strerror(errno));
                    fclose(aesd_file);
                    close(client_sockfd);
                    return -1;
                }
            }
            // Close the file after reading
            fclose(aesd_file);

            // Reopen the file in append mode for the next data packet
            aesd_file = fopen(FILE_PATH, "a");
            if (!aesd_file) {
                syslog(LOG_ERR, "Failed to reopen file in append mode: %s", strerror(errno));
                close(client_sockfd);
                return -1;
            }

        } else {
            // Handle the case where no newline is found in the received data
            fputs(recv_buffer, aesd_file);
            fflush(aesd_file);       // Force the fputted data to be written disk
        }
    }

    if (bytes_received < 0) {
        syslog(LOG_ERR, "Failed to receive data: %s", strerror(errno));
    }

    fclose(aesd_file);

    return 0;
}

// I have adapted the following code for this assignment:
// https://www.thegeekstuff.com/2012/02/c-daemon-process/
// Learned from details from:
// https://psychocod3r.wordpress.com/2019/03/19/how-to-write-a-daemon-process-in-c/
void run_program_in_daemon_mode(){
    pid_t pid = fork();
    pid_t sid = 0;
    if (pid < 0) {
        syslog(LOG_ERR, "Fork failed: %s", strerror(errno));
        free_resources();
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    //unmask the file mode
    umask(0);
    //set new session
    sid = setsid();
    if (sid < 0) {
        syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
        free_resources();
        exit(EXIT_FAILURE);
    }

    if (chdir("/") < 0) {
        syslog(LOG_ERR, "chdir failed: %s", strerror(errno));
        free_resources();
        exit(EXIT_FAILURE);
    }

    // Close stdin. stdout and stderr
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Enable syslogging for daemon process
    openlog(NULL, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);
}