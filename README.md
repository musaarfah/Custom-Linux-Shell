# Custom UNIX Shell

This is a custom UNIX shell created as part of an Operating Systems assignment. The shell includes functionalities from six versions, each building upon the previous to add more features. The shell supports command execution, redirection, piping, background processes, command history, built-in commands, and user-defined variables.

## Features

### Version 1
- Basic shell that allows command execution.
- Displays a prompt and waits for user input.
- Executes commands entered by the user and supports termination with `<CTRL+D>`.

### Version 2
- Adds support for **input/output redirection**.
  - `mycmd < infile > outfile`: Executes `mycmd` with input from `infile` and output to `outfile`.
- Supports **piping** between commands.
  - Example: `cat file1.txt | grep "text"`

### Version 3
- **Background Process Execution**: Run commands in the background using `&` at the end.
  - Example: `sleep 5 &` runs `sleep` in the background.

### Version 4
- **Command History**: Stores the last 10 commands entered.
- Repeat a command by typing `!number`, where `number` is the command's position in history.
  - `!-1` repeats the last command.

### Version 5
- **Built-In Commands**:
  - `cd <directory>`: Change directory.
  - `exit`: Exit the shell.
  - `jobs`: List background jobs.
  - `kill <PID>`: Terminate a background job by its process ID.
  - `help`: Display a list of built-in commands.

### Version 6
- **Variable Support**:
  - Assign and retrieve user-defined variables.
    - Example: `myvar=123` and `echo $myvar` displays `123`.
  - `listvars`: List all user-defined variables.
  - `printenv`: Display all environment variables.

## Getting Started

### Prerequisites
- **GCC Compiler**: Ensure `gcc` is installed on your system for compilation.

### Compilation
To compile the shell, run the following command:
```bash
gcc myshell.c -o myshell



### Basic Commands
Some examples of commands you can run in this shell:

```bash

# Version 1
ls -l             # Lists files in long format
pwd               # Prints the current working directory

## Version 2
cat < input.txt > output.txt   # Reads from input.txt and writes to output.txt
cat /etc/passwd | grep "root"  # Filters lines containing "root" in /etc/passwd

# Version 3
sleep 10 &       # Runs sleep in the background

## version 4
!1               # Repeats the first command in history
!-1              # Repeats the last command

## Version 5
cd /path/to/dir   # Changes the directory
exit              # Exits the shell
jobs              # Lists all background jobs
kill <PID>        # Terminates a background job by PID

## Version 6
myvar=123         # Sets a variable
echo $myvar       # Prints the value of myvar
listvars          # Lists all user-defined variables
printenv          # Displays environment variables

