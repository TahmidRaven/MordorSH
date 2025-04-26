#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h> 
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
// #include <readline/readline.h>
// #include <readline/history.h> 



#define MAX_CMD_LEN 1024
#define MAX_ARGS 128
#define HISTORY_SIZE 100

char *history[HISTORY_SIZE];
int history_count = 0;
pid_t child_pid = -1;


void show_man() {
    printf("\n--- MordorSH MANUAL ---\n");
    printf("Created by: TahmidRaven\n\n");
    printf("Visit my GitHub: https://github.com/TahmidRaven\n");
    printf("--------------------------------------------------\n");
    printf("MordorSH is a Lord of The Rings shell that supports:\n");
    printf("  - Any valid system command (e.g., mkdir,cd, ls, pwd, echo, etc.)\n");
    printf("  - Ctrl+C             : Interrupt the current command\n");
    printf("  - exit               : Exit the shell\n");
    printf("  - clear              : Clear the screen\n");
    printf("  - up arrow           : Show previous command\n");
    printf("  - down arrow         : Show next command\n");
    printf("  - history            : Show command history\n");
    printf("  - Input Redirection  : command < input.txt\n");
    printf("  - Output Redirection : command > output.txt\n");
    printf("  - Append Output      : command >> output.txt\n");
    printf("  - Piping             : command1 | command2\n");
    printf("  - Sequencing         : command1 ; command2\n");
    printf("  - Conditional (AND)  : command1 && command2\n");
    printf("  - man                : Show this manual page\n");
    printf("  - neofetch           : Show system information\n");
    printf("--------------------------\n\n");
}


void print_prompt() {
    char cwd[1024];
    char hostname[1024];
    char *username = getenv("USER");

    getcwd(cwd, sizeof(cwd));
    gethostname(hostname, sizeof(hostname));

    printf("%s@%s:%s MordorSH> ", username, hostname, cwd);
    fflush(stdout);
}

void sigint_handler(int sig) {
    if (child_pid > 0) {
        kill(child_pid, SIGINT);
    } else {
        printf("\n");
        print_prompt();
    }
}


void add_to_history(const char *cmd) {
    if (history_count < HISTORY_SIZE) {
        history[history_count++] = strdup(cmd);
    }
}

void print_hist101() {
    for (int i = 0; i < history_count; i++) {
        printf("%d: %s\n", i + 1, history[i]);  
        printf("\n");
    }
}

void trim_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n')
        str[len - 1] = '\0';
}
#include <stdio.h>
#include <stdlib.h>

void print_sys_cmd_output(const char *cmd, const char *label) {
    char buffer[128];
    printf("%s: ", label);
    FILE *file = popen(cmd, "r");
    if (file) {
        while (fgets(buffer, sizeof(buffer), file)) {
            printf("%s", buffer);
        }
        fclose(file);
    }
}

void print_neofetch() {
    printf("\n--- Tribute to neofetch; RIP ---\n"); 
    printf("\n--- System Information ---\n");
    printf("-------------------------\n");

    print_sys_cmd_output("grep ^PRETTY_NAME /etc/os-release | cut -d= -f2 | tr -d '\"'", "OS");
    print_sys_cmd_output("uname -r", "Kernel");
    print_sys_cmd_output("lscpu | grep 'Model name' | cut -d: -f2 | xargs", "CPU");
    print_sys_cmd_output("free -h | grep Mem | awk '{print $3 \" / \" $2}'", "Memory");
    print_sys_cmd_output("uptime -p", "Uptime");
    print_sys_cmd_output("hostname", "Hostname");
    print_sys_cmd_output("df -h --total | grep total | awk '{print $3 \" / \" $2}'", "Disk");

    printf("\n-------------------------\n");
}

char **parse_args(char *line) {
    char **args = malloc(MAX_ARGS * sizeof(char *));
    int i = 0;
    char *token = strtok(line, " ");
    while (token) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;
    return args;
}

int execute_command(char *cmd);

int handle_redirection_and_execute(char *cmd) {
    char *input_file = NULL, *output_file = NULL;
    int append = 0;

    if (strstr(cmd, ">>")) {
        output_file = strstr(cmd, ">>") + 2;
        append = 1;
    } else if (strchr(cmd, '>')) {
        output_file = strchr(cmd, '>') + 1;
    }

    if (strchr(cmd, '<')) {
        input_file = strchr(cmd, '<') + 1;
    }

    if (input_file) *(strchr(cmd, '<')) = '\0';  // Remove input redirection
    if (output_file) *(strchr(cmd, '>')) = '\0'; // Remove output redirection

    // Trimming whitespace
    if (input_file) input_file = strtok(input_file, " \t\n");
    if (output_file) output_file = strtok(output_file, " \t\n");

    char **args = parse_args(cmd);
    pid_t pid = fork();
    if (pid == 0) {
        if (input_file) {
            int fd = open(input_file, O_RDONLY);
            if (fd < 0) perror("open");
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (output_file) {
            int fd = open(output_file, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0644);
            if (fd < 0) perror("open");
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        execvp(args[0], args);
        perror("execvp");
        exit(1);
    } else {
        child_pid = pid;
        waitpid(pid, NULL, 0);
        child_pid = -1;
    }
    free(args);
    return 0;
}

int handle_pipings(char *cmd) {
    char *commands[32];
    int num_cmds = 0;
    char *token = strtok(cmd, "|");
    while (token) {
        commands[num_cmds++] = token;
        token = strtok(NULL, "|");
    }

    int prev_fd = 0;
    for (int i = 0; i < num_cmds; i++) {
        int pipefd[2];
        pipe(pipefd);

        pid_t pid = fork();
        if (pid == 0) {
            dup2(prev_fd, 0);
            if (i < num_cmds - 1) {
                dup2(pipefd[1], 1);
            }
            close(pipefd[0]);
            char **args = parse_args(commands[i]);
            execvp(args[0], args);
            perror("execvp");
            exit(1);
        } else {
            wait(NULL);
            close(pipefd[1]);
            prev_fd = pipefd[0];
        }
    }
    return 0;
}

int execute_command(char *cmd) {
    if (strchr(cmd, '|')) {
        return handle_pipings(cmd);
    }
    return handle_redirection_and_execute(cmd);
}

void handle_user_commands(char *line) {
    add_to_history(line);

    char *seq_saveptr, *and_saveptr;
    char *seq_token = strtok_r(line, ";", &seq_saveptr);

    while (seq_token) {
        char *and_token = strtok_r(seq_token, "&&", &and_saveptr);
        int last_status = 0;
        while (and_token) {
            trim_newline(and_token);
            if (strcmp(and_token, "exit") == 0) {
                exit(0);
            } else if (strcmp(and_token, "history") == 0) {
                print_hist101();
            } else if (strcmp(and_token, "man") == 0) {
                show_man();
            } else if (strcmp(and_token, "clear") == 0) {
                printf("\033[H\033[J"); 
            } else if (strcmp(and_token, "neofetch") == 0) {
                print_neofetch();
            } 
            else {
                char *cmd_copy = strdup(and_token); 
                char **args = parse_args(cmd_copy);
            
                if (args[0] == NULL) { // empty cli; do nothing
                } else if (strcmp(args[0], "cd") == 0) {
                    if (args[1] == NULL) {
                        fprintf(stderr, "cd: missing argument\n");
                    } else {
                        if (chdir(args[1]) != 0) {
                            perror("cd");
                        }
                    }
                } else if (strcmp(args[0], "mkdir") == 0) {
                    pid_t pid = fork();
                    if (pid == 0) {
                        execvp("mkdir", args);
                        perror("mkdir");
                        exit(1);
                    } else {
                        wait(NULL);
                    }
                } else if (strcmp(args[0], "grep") == 0) {
                    pid_t pid = fork();
                    if (pid == 0) {

                        char *new_args[MAX_ARGS];
                        new_args[0] = "grep";
                        new_args[1] = "--color=auto";   
            
                        int i = 1;
                        while (args[i] != NULL) {
                            new_args[i + 1] = args[i];
                            i++;
                        }
                        new_args[i + 1] = NULL;
            
                        execvp("grep", new_args);
                        perror("grep");
                        exit(1);
                    } else {
                        wait(NULL);
                    }
                } else {
                    last_status = execute_command(and_token);
                }
            
                free(cmd_copy);
                free(args);
            }
            
            
            

            if (last_status != 0) break;
            and_token = strtok_r(NULL, "&&", &and_saveptr);
        }
        seq_token = strtok_r(NULL, ";", &seq_saveptr);
    }
}

void enable_raw_mode(struct termios *original) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, original);
    raw = *original;
    raw.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode(struct termios *original) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, original);
}



int main() {
    char input[MAX_CMD_LEN];
    int input_len = 0;
    int history_index = -1;

    struct termios original_term;
    enable_raw_mode(&original_term);

    signal(SIGINT, sigint_handler);

    printf(   // doom ascii art 
        "    ___  ___              _            _____ _   _ \n"
        "    |  \\/  |             | |          /  ___| | | |\n"
        "    | .  . | ___  _ __ __| | ___  _ __\\ `--.| |_| |\n"
        "    | |\\/| |/ _ \\| '__/ _` |/ _ \\| '__|`--. \\  _  |\n"
        "    | |  | | (_) | | | (_| | (_) | |  /\\__/ / | | |\n"
        "    \\_|  |_/\\___/|_|  \\__,_|\\___/|_|  \\____/\\_| |_/\n"
        "                                                  \n"
        "        Welcome to MordorSH — The Dark Shell      \n"
        "       “One Shell to Rule Them All...”            \n"
        );
        
        printf("--------------------------------------------------\n");
        
        printf("Created by: TahmidRaven\n\n");
        printf("Visit my GitHub: https://github.com/TahmidRaven\n");
        printf("--------------------------------------------------\n");

        printf("\n");

    printf("Type \"man\" to see the list of available commands.\n\n");

    while (1) {
        print_prompt();

        fflush(stdout);
        input_len = 0;
        input[0] = '\0';

        int ch;
        while ((ch = getchar()) != '\n') {
            if (ch == 127 || ch == 8) { // to handle backspace
                if (input_len > 0) {
                    input_len--;
                    input[input_len] = '\0';
                    printf("\b \b");
                    fflush(stdout);
                }
            } else if (ch == 27) { // ESC
                char next1 = getchar();
                if (next1 == '[') {
                    char next2 = getchar();
                    if (next2 == 'A') { // UP arrow
                        if (history_count > 0 && history_index < history_count - 1) {
                            history_index++;
                            strcpy(input, history[history_count - 1 - history_index]);
                            input_len = strlen(input);
                            printf("\33[2K\r"); // clear line
                            
                            print_prompt();  
                            printf("%s", input);  
                            fflush(stdout);
                        }
                    } else if (next2 == 'B') { // DOWN arrow
                        if (history_index > 0) {
                            history_index--;
                            strcpy(input, history[history_count - 1 - history_index]);
                            input_len = strlen(input);
                        } else {
                            input[0] = '\0';
                            input_len = 0;
                        }
                        printf("\33[2K\r"); // clear line
                        print_prompt();  
                        printf("%s", input); 
                        fflush(stdout);
                    }
                    
                }
            } else {
                if (input_len < MAX_CMD_LEN - 1) {
                    input[input_len++] = ch;
                    input[input_len] = '\0';
                    putchar(ch);
                    fflush(stdout);
                }
            }
        }

        printf("\n");

        if (input_len == 0) continue;
        history_index = -1;
        handle_user_commands(input);
    }

    disable_raw_mode(&original_term);
    return 0;
}