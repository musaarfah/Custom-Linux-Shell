#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "PUCITshell:- "

int execute(char* arglist[]);
char** tokenize(char* cmdline);
char* read_cmd(char*, FILE*);
int handle_redirection(char** arglist);
int handle_pipe(char* cmdline);

int main() {
    char *cmdline;
    char** arglist;
    char* prompt = PROMPT;   
    while ((cmdline = read_cmd(prompt, stdin)) != NULL) {
        // Check if command contains a pipe
        if (strstr(cmdline, "|")) {
            handle_pipe(cmdline);
        } else {
            if ((arglist = tokenize(cmdline)) != NULL) {
                execute(arglist);
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

int execute(char* arglist[]) {
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
            waitpid(cpid, &status, 0);
            printf("child exited with status %d \n", status >> 8);
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

    while (token != NULL && index < 2) {
        commands[index++] = token;
        token = strtok(NULL, "|");
    }

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

    pid_t pid1 = fork();
    if (pid1 == 0) {
        // First child process - executes the first command
        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
        close(pipefd[0]); // Close unused read end
        close(pipefd[1]);

        // Tokenize and execute the first command
        char** arglist1 = tokenize(commands[0]);
        if (arglist1 != NULL) {
            execvp(arglist1[0], arglist1);
            perror("Command not found...");
            exit(1);
        }
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        // Second child process - executes the second command
        dup2(pipefd[0], STDIN_FILENO); // Redirect stdin from pipe
        close(pipefd[1]); // Close unused write end
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

