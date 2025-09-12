#include "systemcalls.h"

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    int retVal = false;
    
    retVal = system(cmd);

    // if retval is -1 it is an error, so if it is not -1, return true (no error)
    return (retVal != -1);
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
    pid_t process_id;
    int wstatus;
    int retVali = 0;
    bool retvalb = true;

    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    process_id = fork();

    // pid of -1 means it errored out
    if(process_id == -1)
    {
        return false;
    }
    // pid of 0 means child
    else if(process_id == 0)
    {
        retVali = execv(command[0], command);
        if(retVali == -1)
        {
            exit(1);
        }
    }
    // pid of non zero means parent
    else
    {
        if(wait(&wstatus) == -1)
        {
            retvalb = false;
        }
        
        
        // if the child returned or exited
        if(WIFEXITED(wstatus))
        {
            // if the child returned with something other then 0
            if(WEXITSTATUS(wstatus))
            {
                retvalb = false;
            }
        }
    }

    va_end(args);

    return retvalb;
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
    pid_t process_id;
    int wstatus;
    int retVali = 0;
    bool retvalb = true;

    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }

    command[count] = NULL;

    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);

    process_id = fork();

    // pid of -1 means it errored out
    if(process_id == -1)
    {
        return false;
    }
    // pid of 0 means child
    else if(process_id == 0)
    {
        if(dup2(fd, STDOUT_FILENO) == -1)
        {
            exit(1);
        }
        close(fd);
        retVali = execv(command[0], command);
        if(retVali == -1)
        {
            exit(1);
        }
    }
    // pid of non zero means parent
    else
    {
        if(wait(&wstatus) == -1)
        {
            retvalb = false;
        }
        
        
        // if the child returned or exited
        if(WIFEXITED(wstatus))
        {
            // if the child returned with something other then 0
            if(WEXITSTATUS(wstatus))
            {
                retvalb = false;
            }
        }
    }

    close(fd);

    va_end(args);

    return retvalb;
}
