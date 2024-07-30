#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

/* Implement an almost equivalent implementation of writer.sh
   Only difference being do not need to create directory if it does not exist. Assume it exists.
*/

int main(int argc, char *argv[]) {
    openlog("writer.c",LOG_PERROR | LOG_PID,LOG_USER);

    // Check if exactly two arguments were provided to the program
    // must check argc == 3 since:
    // argv[0] = program name
    // argv[1] = file path
    // argv[2] = string to write to file
    if(argc!=3){
        syslog(LOG_ERR,"Error: Must provide exactly two arguments.");
        syslog(LOG_ERR,"Error: Usage: %s <writefile> <writestr>",argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *writefile = argv[1];
    const char *writestr = argv[2];
    
    // Open the file in create mode for writing
    // We must provide the O_TRUNC flag to ensure the overwriting of the file if it already exists
    int fd = open(writefile, O_CREAT|O_WRONLY|O_TRUNC, 0644);
    if(fd==-1){
        syslog(LOG_ERR, "Error: Could not open file %s with open(): %s", writefile, strerror(errno));
        //close(fd);  //We can't close something that couldn't open in the first place
        exit(EXIT_FAILURE);
    }

    // Write the content to file
    ssize_t write_bytes = write(fd, writestr, strlen(writestr));
    if(write_bytes==-1){
        syslog(LOG_ERR, "Error: Could not write to file %s with write(): %s", writefile, strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }

    // We got here, so a successful write
    syslog(LOG_DEBUG, "Success: Wrote %s to file named: %s", writestr, writefile);

    // Close the file gracefully
    int close_val = close(fd);
    if(close_val==-1){
        syslog(LOG_ERR, "Error: Could not close file %s: %s", writefile, strerror(errno));
        exit(EXIT_FAILURE);
    }

    closelog();

    return EXIT_SUCCESS;
}