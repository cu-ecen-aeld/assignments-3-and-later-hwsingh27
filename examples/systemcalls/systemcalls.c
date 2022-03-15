/**
 *@file: systemcalls.c
 *@author: Harshwardhan Singh (harshwardhan.singh@colorado.edu)
 *@institute: University of Colorado Boulder
 *@course: Advanced Embedded Software Development (ECEN-5713)
 *@date: 01/29/2022
 *@reference: Video lectures by Prof. Daniel Walkes
              Linux Programming Interface by Michael Kerrisk
              Linux System Programming by Robert Love
*/

#include "systemcalls.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <syslog.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the commands in ... with arguments @param arguments were executed 
 *   successfully using the system() call, false if an error occurred, 
 *   either in invocation of the system() command, or if a non-zero return 
 *   value was returned by the command issued in @param.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success 
 *   or false() if it returned a failure
*/
    int system_ret;
    
    openlog(NULL, 0, LOG_USER);
    system_ret = system(cmd);
    if (system_ret == -1)
    {
	syslog(LOG_ERR, "Failure: System error");
    	return false;
    }
    if(system_ret != -1)
    {
	syslog(LOG_ERR, "Shell is available");
	return false;
    }
    return true;
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
    pid_t pid, wait_result;

    openlog(NULL, 0, LOG_USER);

    pid = fork();
    
    if (pid == -1)
    {
    	syslog(LOG_ERR, "Failure: Fork failed");
    	closelog();
    	return false;
    }
    
    else if (pid == 0)
    {
    	syslog(LOG_ERR, "Child process created");
    	execv(command[0], command);
    	closelog();
    	exit(EXIT_FAILURE);
    }
    
    else
    {
    	wait_result = waitpid(pid, &status, 0);
    	if(wait_result == -1)
    	{
            syslog(LOG_ERR, "Error in waiting for termination process");
            closelog();
            return false;
        }
        
        if (!(WIFEXITED(status)))
        {
            syslog(LOG_ERR, "Failure in waitpid");
            closelog();
            return false;
        }
        if (WEXITSTATUS(status))
        {
            syslog(LOG_ERR, "Failure in waitpid");
            closelog();
            return false;    	
        }
    }

    va_end(args);
    closelog();
    return true;
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
    openlog(NULL, 0, LOG_USER);
    
    int status, fd;
    pid_t pid, wait_result;

    fd = open(outputfile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if(fd == -1)
    {
        syslog(LOG_ERR, "Failure: file cannot be opened");
        closelog();
        return false;
    }

    pid = fork();
    
    if (pid == -1)
    {
	syslog(LOG_ERR, "Failure: Child process is not created");
    	closelog();
    	return false;
    }
    
    else if (pid == 0)
    { 
        if(dup2(fd, STDOUT_FILENO) < 0)
        {
            syslog(LOG_ERR, "Failure: duplication of file failed");
            closelog();
            return false;
        }
        
        close(fd);
        execv(command[0],command);
        closelog();
        exit(EXIT_FAILURE);
    }
    else
    {
	close(fd);
    	wait_result = waitpid(pid, &status, 0);
    	if(wait_result == -1)
    	{
            syslog(LOG_ERR, "Error in waiting for termination process");
            closelog();
            return false;
        }
        
        if (!(WIFEXITED(status)))
        {
            syslog(LOG_ERR, "Failure in waitpid");
            closelog();
            return false;
        }
        if (WEXITSTATUS(status))
        {
            syslog(LOG_ERR, "Failure in waitpid");
            closelog();
            return false;    	
        }

    }

    va_end(args);
    closelog();
    return true;
}
