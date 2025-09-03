#include <stdio.h>
#include <syslog.h>
#include <string.h>

// A function to simplify the setup of the syslog
void setup_syslog()
{
    // null means the program name is appeneded to the front of each msg
    // specify no additional log flags
    // tell the logger where we are logging from (user space)
    openlog(NULL, 0, LOG_USER);
    
}

// The entire writer application from assignment 1
int writer_app(char *file_path, char *text_to_write, unsigned int text_length)
{
    FILE *fp;

    syslog(LOG_DEBUG, "Writing %s to %s", text_to_write, file_path);

    fp = fopen(file_path, "w");

    if(fp == NULL)
    {
        syslog(LOG_ERR, "Error when opening file: %s", file_path);
        return 1;
    }

    fwrite(text_to_write, text_length, 1, fp);
    
    fclose(fp);

    return 0;
}

// runs all setup functions then the main application
int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        syslog(LOG_ERR, "Not enough arguments passed in, please provide 2, given %d", argc-1);
        return 1;
    }

    char *file_path = argv[1];
    char *text_to_write = argv[2];
    int length = strlen(text_to_write);

    setup_syslog();

    // return with an error if an error occured in the writer application
    if(writer_app(file_path, text_to_write, length))
    {
        return 1;
    }

    return 0;
}

