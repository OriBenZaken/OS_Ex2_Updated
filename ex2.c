#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <wait.h>
#include <stdbool.h>

#define MAX_COMMAND_ARGS 10
#define MAX_COMMAND_LENGTH 100
#define MAX_JOBS_NUMBER 50
#define DONT_WAIT 0
#define WAIT 1
#define FAILURE -1
#define NO_JOB -2
#define DIRECTROY_PATH_SIZE 1024


typedef struct job {
    char command[MAX_COMMAND_LENGTH];
    pid_t pid;
} job;

/**
 * intializes the jobs array
 * @param jobs
 */
void initalizeJobsArray(job* jobs) {
    int i;
    for (i = 0; i < MAX_JOBS_NUMBER; i++) {
        strcpy(jobs[i].command, "");
        jobs[i].pid = NO_JOB;
    }
}

/**
 * insert a command (job) into the jobs array
 * @param jobs jobs array
 * @param pid process pid
 * @param command command
 * @return 1 - insertion succeeded, 0 - insertion failed
 */
int insertToJobsArray(job* jobs, pid_t pid, char* command) {
    int i = 0;
    int status;
    while (i < MAX_JOBS_NUMBER) {
        // index is availabe or the job in the index isn't running anymore
        if (jobs[i].pid == NO_JOB || waitpid(jobs[i].pid, &status, WNOHANG) != 0) {
            jobs[i].pid = pid;
            strcpy(jobs[i].command, command);
            break;
        }
        i++;
    }
    if (i == MAX_JOBS_NUMBER) {
        return 0;
    }
    return 1;
}

/**
 * prints to the console all the running jobs.
 * @param jobs jobs array
 */
void displayJobs(job* jobs) {
    int i, status;
    for (i = 0; i < MAX_JOBS_NUMBER; i++) {
        // pid has a meaningful value and the process is running
        if (jobs[i].pid != NO_JOB && waitpid(jobs[i].pid, &status, WNOHANG) == 0) {
            printf("%d %s\n", jobs[i].pid, jobs[i].command);
        } else {
            jobs[i].pid = NO_JOB;
        }
    }
}

/***
 * split the command string to tokens
 * @param args char* array - for the command name and arguments
 * @param command command
 * @param waitFlag background command flag
 */
void stringToExecvArgs(char** args, char* command, int* waitFlag) {
    char* token = strtok(command, " ");
    int i = 0;
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    // command with '&' in the end is a background command
    if (strcmp(args[i - 1],"&") == 0) {
        *waitFlag = DONT_WAIT;
        args[i - 1] = NULL;
    } else {
        args[i] = NULL;
    }
}

void changeDirectory(char** args, char* previous_wd) {
    // command is: 'cd' or 'cd ~'. Go to home directory
    int successCd;
    if ((args[1] ==  NULL) || (!strcmp(args[1], "~") && args[2] == NULL)) {
        //get current working directory
        getcwd(previous_wd, DIRECTROY_PATH_SIZE);
        successCd = chdir(getenv("HOME"));
    } else if (!strcmp(args[1], "-") && args[2] == NULL) {
        char cwd[DIRECTROY_PATH_SIZE];
        getcwd(cwd, DIRECTROY_PATH_SIZE);
        successCd = chdir(previous_wd);
        if (successCd == 0) {
            printf("%s\n", previous_wd);
        }
        strcpy(previous_wd, cwd);
    } else {
        //get current working directory
        getcwd(previous_wd, DIRECTROY_PATH_SIZE);
        successCd = chdir(args[1]);
    }
    if (successCd != 0) {
        fprintf(stderr, "Failed to execute %s\n", args[0]);
    }
}

/***
 * Executes the command. besides special commands, a new process is created and than call execvp
 * to actually run the command code
 * @param jobs jobs array
 * @param command command
 * @param args command split to tokens
 * @param waitFlag background command flag
 * @param previous_wd previous working directory
 */
void executeCommand(job* jobs, char* command, char** args, int waitFlag, char* previous_wd) {
    int retVal;
    // special commands implementation
    if (!strcmp(args[0], "exit")) {
        printf("%d\n", getpid());
        exit(0);
    }
    if (!strcmp(args[0], "jobs") && args[1] == NULL) {
        displayJobs(jobs);
        return;
    } else if (!strcmp(args[0], "cd")) {
        printf("%d\n", getpid());
        changeDirectory(args, previous_wd);
        return;
    }
    pid_t pid;
    // create son process to execute the command
    pid = fork();
    if (pid < 0) {
        printf("fork error\n");
        return;
    }
    // main process prints the child process pid
    if (pid != 0) {
        printf("%d\n",pid);
    }
    // son process
    if (pid == 0) {
        int i = 0;
        retVal = execvp(args[0], args);
        if (retVal == FAILURE) {
            // execution failed. writing to STDERR
            fprintf(stderr, "Error in system call\n");
            exit(FAILURE);
        }
        // main process
    } else {
        // not a background command
        if (waitFlag) {
            waitpid(pid, NULL, 0);
            // background command - add to jobs array
        } else {
            command[strlen(command) - 1] = '\0';
            int jobInsertSuccess = insertToJobsArray(jobs, pid, command);
            if (!jobInsertSuccess) {
                printf("failed to insert into jobs array\n");
            }
        }
    }
}
/***
 * Shell implementation
 * @return 0
 */
int main() {
    char command[MAX_COMMAND_LENGTH];   // command string input
    char* args[MAX_COMMAND_ARGS];   // command split into tokens
    job jobs[MAX_JOBS_NUMBER];  // jobs array
    char previous_wd[DIRECTROY_PATH_SIZE];  // keeps the previous working directory

    initalizeJobsArray(jobs);

    while (true) {
        printf("prompt> ");
        fgets(command, MAX_COMMAND_LENGTH, stdin);
        // remove new line character
        command[strlen(command) - 1] = '\0';
        // empty line
        if (command[0] == '\0') {
            continue;
        }
        char commandCpy[MAX_COMMAND_LENGTH];
        // copy command for the jobs array
        strcpy(commandCpy, command);
        // background command flag - set to NOT background flag by default
        int waitFlag = WAIT;
        stringToExecvArgs(args, command, &waitFlag);
        // execute the command
        executeCommand(jobs, commandCpy, args, waitFlag, previous_wd);
        if (!strcmp(args[0], "cat")) {
            printf("\n");
        }
    }
    return 0;
}