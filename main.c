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

//global variable - saves the previous  working directory


typedef struct job {
    char command[MAX_COMMAND_LENGTH];
    pid_t pid;
} job;

void initalizeJobsArray(job* jobs) {
    int i;
    for (i = 0; i < MAX_JOBS_NUMBER; i++) {
        strcpy(jobs[i].command, "");
        jobs[i].pid = NO_JOB;
    }
}

int insertToJobsArray(job* jobs, pid_t pid, char* command) {
    int i = 0;
    int status;
    while (i < MAX_JOBS_NUMBER) {
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

void displayJobs(job* jobs) {
    int i, status;
    for (i = 0; i < MAX_JOBS_NUMBER; i++) {
        if (jobs[i].pid != NO_JOB && waitpid(jobs[i].pid, &status, WNOHANG) == 0) {
            printf("%d %s\n", jobs[i].pid, jobs[i].command);
        } else {
            jobs[i].pid = NO_JOB;
        }
    }
}

void stringToExecvArgs(char** args, char* command, int* waitFlag) {
    char* token = strtok(command, " ");
    int i = 0;
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
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

void executeCommand(job* jobs, char* command, char** args, int waitFlag, char* previous_wd) {
    int retVal;
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
        if (waitFlag) {
            waitpid(pid, NULL, 0);
        } else {
            command[strlen(command) - 1] = '\0';
            int jobInsertSuccess = insertToJobsArray(jobs, pid, command);
            if (!jobInsertSuccess) {
                printf("failed to insert into jobs array\n");
            }
        }
    }
}

int main() {

    char command[MAX_COMMAND_LENGTH];
    char* args[MAX_COMMAND_ARGS];
    job jobs[MAX_JOBS_NUMBER];
    char previous_wd[DIRECTROY_PATH_SIZE];
    char wd[DIRECTROY_PATH_SIZE];
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
        strcpy(commandCpy, command);

        int waitFlag = WAIT;
        stringToExecvArgs(args, command, &waitFlag);
        executeCommand(jobs, commandCpy, args, waitFlag, previous_wd);
        if (!strcmp(args[0], "cat")) {
            printf("\n");
        }
    }
    return 0;
}