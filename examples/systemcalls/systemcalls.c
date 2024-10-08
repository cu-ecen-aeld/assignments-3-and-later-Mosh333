#include "systemcalls.h"
#include <stdlib.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>  // For fork, execv, close, dup2

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    printf("Executing command: %s\n", cmd);
    int status = system(cmd);
    if(status==-1){
        return false;
    }

    if (WIFEXITED(status)) {
        printf("Exited with status: %d\n", WEXITSTATUS(status));
    } else {
        printf("Child process did not exit normally.\n");
    }

    //https://www.gnu.org/software/libc/manual/html_node/Process-Completion-Status.html
    //man system, man exit, man waitpid
    if(WIFEXITED(status) && WEXITSTATUS(status) == 0){
        //confirm success as per "man system"
        return true;
    }

    return false;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    int status;
    pid_t pid = fork();
    if(pid == -1){
        // Fork failed
        return false;
    }
    else if(pid == 0){
        // Child process
        execv(command[0], command);
        // If execv returns, it must have failed
        exit(1);
    } else {
        // Parent process
        if(waitpid(pid, &status, 0) == -1){
            return false;
        }
        return WEXITSTATUS(status) == 0;
    }
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
    int fd = open(outputfile, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (fd < 0) {
        perror("open");
        va_end(args);
        return false;
    }

    pid_t kidpid = fork();
    if (kidpid == -1) {
        // Fork failed
        perror("fork");
        close(fd);
        va_end(args);
        return false;
    }

    if (kidpid == 0) {
        // Child process
        if (dup2(fd, 1) < 0) {
            // Redirect stdout to the file
            perror("dup2");
            close(fd);
            va_end(args);
            exit(1);
        }
        close(fd);
        execv(command[0], command);
        // If execv returns, it must have failed
        perror("execv");
        return false;
    } else {
        // Parent process
        close(fd);
        int status;
        if (waitpid(kidpid, &status, 0) == -1) {
            // waitpid failed
            perror("waitpid");
            va_end(args);
            return false;
        }

        va_end(args);

        // Check if the child process terminated normally and its exit status is 0
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            return true;
        } else {
            return false;
        }
    }
}