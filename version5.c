#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "MUSAshell:- "
#define HISTORY_SIZE 10
#define MAX_JOBS 10

int execute(char* arglist[], int background);
char** tokenize(char* cmdline);
char* read_cmd(char*, FILE*);
int handle_redirection(char** arglist);
int handle_pipe(char* cmdline);
void handle_sigchld(int sig);
void trim_whitespace(char* str);
void add_to_history(char* command);
int execute_builtin(char** arglist);
void list_jobs();
void remove_job(pid_t pid);
void add_job(pid_t pid, char* command);

// Structure to keep track of background jobs
typedef struct {
    pid_t pid;
    char command[MAX_LEN];
} Job;

// Global variables for command history and jobs
char* history[HISTORY_SIZE];
int history_count = 0;
Job jobs[MAX_JOBS];
int job_count = 0;

int main() {
    // Initialize history and jobs
    for (int i = 0; i < HISTORY_SIZE; i++) {
        history[i] = NULL;
    }

    signal(SIGCHLD, handle_sigchld);

    char *cmdline;
    char** arglist;
    char* prompt = PROMPT;   
    while ((cmdline = read_cmd(prompt, stdin)) != NULL) {
        // Trim any leading or trailing whitespace
        trim_whitespace(cmdline);

        // Check if the user wants to repeat a command using `!number`
        if (cmdline[0] == '!') {
            int history_index;
            if (cmdline[1] == '-') {
                // Handle the case of !-1 (repeat last command)
                history_index = history_count - 1;
            } else {
                // Convert the number to an integer
                history_index = atoi(&cmdline[1]) - 1;
            }

            if (history_index >= 0 && history_index < HISTORY_SIZE && history[history_index] != NULL) {
                printf("Repeating command: %s\n", history[history_index]);
                strcpy(cmdline, history[history_index]);
            } else {
                printf("Invalid history number!\n");
                continue;
            }
        } else {
            // Add the command to history
            add_to_history(cmdline);
        }

        int background = 0;
        int len = strlen(cmdline);

        // Check if the command should run in the background
        if (len > 0 && cmdline[len - 1] == '&') {
            background = 1;
            cmdline[len - 1] = '\0';  // Remove the '&' character
            trim_whitespace(cmdline);  // Remove any trailing whitespace left
        }

        if (strstr(cmdline, "|")) {
            handle_pipe(cmdline);
        } else {
            if ((arglist = tokenize(cmdline)) != NULL) {
                // Check if the command is a built-in command
                if (execute_builtin(arglist) == 0) {
                    execute(arglist, background);
                }
                // Free memory for arglist
                for (int j = 0; j < MAXARGS+1; j++)
                    free(arglist[j]);
                free(arglist);
                free(cmdline);
            }
        }
    }
    printf("\n");
    return 0;
}

int execute(char* arglist[], int background) {
    int status;
    int cpid = fork();

    switch(cpid) {
        case -1:
            perror("fork failed");
            exit(1);
        case 0:
            // Handle redirection if necessary
            handle_redirection(arglist);
            execvp(arglist[0], arglist);
            perror("Command not found...");
            exit(1);
        default:
            if (!background) {
                // If not running in the background, wait for the child to complete
                waitpid(cpid, &status, 0);
                printf("child exited with status %d \n", status >> 8);
            } else {
                // If running in the background, print the process ID and add it to jobs
                printf("[Background process started with PID %d]\n", cpid);
                add_job(cpid, arglist[0]);
            }
            return 0;
    }
}

char** tokenize(char* cmdline) {
    // Allocate memory
    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS+1));
    for (int j = 0; j < MAXARGS+1; j++) {
        arglist[j] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[j], ARGLEN);
    }
    if (cmdline[0] == '\0') // if user has entered nothing and pressed enter key
        return NULL;
    int argnum = 0; // slots used
    char* cp = cmdline; // pos in string
    char* start;
    int len;
    while (*cp != '\0') {
        while (*cp == ' ' || *cp == '\t') // skip leading spaces
            cp++;
        start = cp; // start of the word
        len = 1;
        // find the end of the word
        while (*++cp != '\0' && !(*cp == ' ' || *cp == '\t'))
            len++;
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }
    arglist[argnum] = NULL;
    return arglist;
}      

char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    int c; // input character
    int pos = 0; // position of character in cmdline
    char* cmdline = (char*)malloc(sizeof(char) * MAX_LEN);
    while ((c = getc(fp)) != EOF) {
        if (c == '\n')
            break;
        cmdline[pos++] = c;
    }
    // If user presses CTRL+D to exit the shell
    if (c == EOF && pos == 0) 
        return NULL;
    cmdline[pos] = '\0';
    return cmdline;
}

int handle_redirection(char** arglist) {
    int i = 0;
    while (arglist[i] != NULL) {
        if (strcmp(arglist[i], "<") == 0) {
            int fd = open(arglist[i+1], O_RDONLY);
            if (fd < 0) {
                perror("Failed to open input file");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            arglist[i] = NULL; // Remove redirection part from args
        } else if (strcmp(arglist[i], ">") == 0) {
            int fd = open(arglist[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("Failed to open output file");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            arglist[i] = NULL; // Remove redirection part from args
        }
        i++;
    }
    return 0;
}

int handle_pipe(char* cmdline) {
    // Split the command based on the pipe '|'
    char* commands[2];
    char* token = strtok(cmdline, "|");
    int index = 0;

    // Tokenize the input based on '|'
    while (token != NULL && index < 2) {
        commands[index++] = token;
        token = strtok(NULL, "|");
    }

    // Check if there are exactly two commands for the pipe
    if (index != 2) {
        fprintf(stderr, "Invalid pipe command\n");
        return -1;
    }

    // Create a pipe
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        exit(1);
    }

    // First child process for the command before the pipe
    pid_t pid1 = fork();
    if (pid1 == 0) {
        // Child process 1
        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to the pipe
        close(pipefd[0]); // Close unused read end of the pipe
        close(pipefd[1]);

        // Tokenize and execute the first command
        char** arglist1 = tokenize(commands[0]);
        if (arglist1 != NULL) {
            execvp(arglist1[0], arglist1);
            perror("Command not found...");
            exit(1);
        }
    }

    // Second child process for the command after the pipe
    pid_t pid2 = fork();
    if (pid2 == 0) {
        // Child process 2
        dup2(pipefd[0], STDIN_FILENO); // Redirect stdin from the pipe
        close(pipefd[1]); // Close unused write end of the pipe
        close(pipefd[0]);

        // Tokenize and execute the second command
        char** arglist2 = tokenize(commands[1]);
        if (arglist2 != NULL) {
            execvp(arglist2[0], arglist2);
            perror("Command not found...");
            exit(1);
        }
    }

    // Parent process closes pipe ends and waits for children
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    return 0;
}

void handle_sigchld(int sig) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // Remove completed background processes from jobs
        remove_job(waitpid(-1, NULL, WNOHANG));
    }
    errno = saved_errno;
}

void add_to_history(char* command) {
    if (history_count < HISTORY_SIZE) {
        history[history_count] = strdup(command);
        history_count++;
    } else {
        // Shift commands in history to make space for the new one
        free(history[0]);
        for (int i = 1; i < HISTORY_SIZE; i++) {
            history[i - 1] = history[i];
        }
        history[HISTORY_SIZE - 1] = strdup(command);
    }
}

void trim_whitespace(char* str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return;
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
}

int execute_builtin(char** arglist) {
    if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] != NULL) {
            if (chdir(arglist[1]) != 0) {
                perror("cd failed");
            }
        } else {
            fprintf(stderr, "cd: missing argument\n");
        }
        return 1;
    }
    if (strcmp(arglist[0], "exit") == 0) {
        printf("Exiting shell...\n");
        exit(0);
    }
    if (strcmp(arglist[0], "jobs") == 0) {
        list_jobs();
        return 1;
    }
    if (strcmp(arglist[0], "kill") == 0) {
        if (arglist[1] != NULL) {
            pid_t pid = atoi(arglist[1]);
            if (kill(pid, SIGKILL) == 0) {
                printf("Process %d killed.\n", pid);
            } else {
                perror("Failed to kill process");
            }
        } else {
            fprintf(stderr, "kill: missing PID\n");
        }
        return 1;
    }
    if (strcmp(arglist[0], "help") == 0) {
        printf("Available built-in commands:\n");
        printf("cd <directory> - Change the working directory.\n");
        printf("exit - Terminate the shell.\n");
        printf("jobs - List background processes.\n");
        printf("kill <PID> - Terminate a background process by PID.\n");
        printf("help - Display this help message.\n");
        return 1;
    }
    return 0; // Not a built-in command
}

void list_jobs() {
    printf("Background jobs:\n");
    for (int i = 0; i < job_count; i++) {
        printf("[%d] %s (PID: %d)\n", i + 1, jobs[i].command, jobs[i].pid);
    }
}

void add_job(pid_t pid, char* command) {
    if (job_count < MAX_JOBS) {
        jobs[job_count].pid = pid;
        strncpy(jobs[job_count].command, command, MAX_LEN);
        job_count++;
    } else {
        printf("Job list is full! Unable to add more background processes.\n");
    }
}

void remove_job(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            // Shift remaining jobs down the list
            for (int j = i; j < job_count - 1; j++) {
                jobs[j] = jobs[j + 1];
            }
            job_count--;
            break;
        }
    }
}

