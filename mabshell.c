#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>

#include <errno.h>
#include <signal.h> 
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "mabshell.h"
#include "processes.h"
#include "jobs.h"
#include "utils.h"

#define PATH_MAX 4096

bool has_foreground_process = false;
pid_t foreground_process_id;
char path[PATH_MAX];
JobList job_list;

int main(int argc, char** argv) {

    job_list = new_job_list();
    
    signal(SIGINT, handle_sig_int); 
    signal(SIGTSTP, handle_sig_stop); 

    while(true) {
        getcwd(path, sizeof(path));

        // Writing prompt
        printf("\033[1;32mmabshell\n> \033[0m");
        // Reading a command
        char* line = read_line();

        // Parsing the command line
        CommandLine command_line = parse_command_line(line);

        if(command_line.argument_count != 0) {
            BuiltinCommand builtin = try_get_builtin_command(&command_line);
            if(builtin != NO_BUILTIN_COMMAND) {
                BuiltinCommandFunction command_function = get_builtin_command_function(builtin);
                command_function(&command_line);

            } else {
                pid_t pid = handle_external_command(&command_line);
                if(pid > 0) {
                    add_process_to_job_list(&job_list, pid, line);

                    // Process created with success
                    if(!command_line.run_on_background) {
                        has_foreground_process = true;
                        foreground_process_id = pid;
                    }
                }
            }
        }

        free_command_line(&command_line);

        if(has_foreground_process) {
            int status;

            do {
                if(waitpid(foreground_process_id, &status, WUNTRACED | WCONTINUED) < 0) {
                    perror("mabshell: waitpid(): ");
                    exit(1);
                }

                has_foreground_process = false;
            } while(has_foreground_process);
        }
    }

    return 0;
}

char* read_line() {
    char* result;

    int buffer_size = 1024;
    int offset = 0;

    result = (char*) malloc(buffer_size * sizeof(char));

    bool finished = false;
    while(!finished) {
        char* write_ptr = result + offset;
        
        if(fgets(write_ptr, buffer_size, stdin) == NULL) {
            printf("\nExit: End-of-file\n");
            exit(0);
        }

        char last_char = result[strlen(result) - 1];
        finished = last_char == '\n';

        if(!finished) {
            result = (char*) realloc(result, buffer_size * 2 * sizeof(char));

            // Menos um para sobreescrever o '\0'
            offset += buffer_size - 1;
            buffer_size *= 2;
        }
    }

    return result;
}

CommandLine parse_command_line(char* line) {
    CommandLine result;

    int line_length = strlen(line);
    
    // Removendo o '\n'
    line[line_length - 1] = '\0';
    line_length--;

    while(line[line_length - 1] == ' ') {
        line[line_length - 1] = '\0';
        line_length--;
    }

    if(line[line_length - 1] == '&') {
        result.run_on_background = true;
        // Removendo o '&'
        line[line_length - 1] = '\0';
        line_length--;
    } else {
        result.run_on_background = false;
    }

    result.argument_count = get_argument_count(line);

    if(result.argument_count == 0) {
        return result;
    }

    // Alocando uma posição a mais para inserir uma string de terminação
    result.arguments = (char**) malloc((result.argument_count + 1) * sizeof(char*));

    get_arguments(result.arguments, line);

    // inserindo a string de terminação
    result.arguments[result.argument_count] = NULL;

    return result;
}

void free_command_line(CommandLine* command_line) {
    for(int i = 0; i < command_line->argument_count; i++) {
        free(command_line->arguments[i]);
    }
}

BuiltinCommand try_get_builtin_command(CommandLine* command_line) {
    char* command_name = command_line->arguments[0];

    if(strcmp(command_name, "fg") == 0) {
        return FG;
    }

    if(strcmp(command_name, "bg") == 0) {
        return BG;
    }

    if(strcmp(command_name, "cd") == 0) {
        return CD;
    }

    if(strcmp(command_name, "jobs") == 0) {
        return JOBS;
    }

    if(strcmp(command_name, "pwd") == 0) {
        return PWD;
    }

    if(strcmp(command_name, "ls") == 0) {
        return LS;
    }

    return NO_BUILTIN_COMMAND;
}

BuiltinCommandFunction get_builtin_command_function(BuiltinCommand builtin_command) {
    switch (builtin_command)
    {
    case FG:
        return &handle_fg;
    case BG:
        return &handle_bg;
    case CD:
        return &handle_cd;
    case JOBS:
        return &handle_jobs;
    case PWD:
        return &handle_pwd;
    case LS:
        return &handle_ls;
    default:
        return NULL;
    }
}

void handle_sig_int(int signal) {
    printf("Handling SIGINT\n");
    // exit(0);
}

void handle_sig_stop(int signal) {
    printf("Handling SIGSTP\n");
}

void job_to_foreground(Job job) {
    // TODO: kill SIGCONT
    foreground_process_id = job.process_id;
    has_foreground_process = true;

    puts(job.line);
}

void handle_fg(CommandLine* command_line) {
    if(command_line->argument_count == 1) {
        if(job_list.last == NULL) {
            printf("mabshell: fg : atual : trabalho não existe\n");
        } else {
            job_to_foreground(job_list.last->job);
        }

        return;
    }

    char* arg = command_line->arguments[1];

    Job job;
    if(arg[0] == '%') {
        // Utilizano um JID
        int jid = atoi(arg + 1);
        if(!get_job_with_jid(&job_list, jid, &job)) {
            printf("mabshell: fg : %s : trabalho não existe\n", arg);
            return;
        }
    } else {
        // Utilizano um PID
        pid_t pid = atoi(arg);
        if(!get_job_with_pid(&job_list, pid, &job)) {
            printf("mabshell: fg : %s : trabalho não existe\n", arg);
            return;
        }
    }

    job_to_foreground(job);
}

void handle_bg(CommandLine* command_line) {
    printf("Handling bg\n");
}

void handle_pwd(CommandLine* command_line) {
    printf("%s\n", path);
}

void handle_ls(CommandLine* command_line) {
    struct dirent** filelist;
    int n;
    if(command_line->argument_count == 1) {
        n = scandir(".", &filelist, NULL, alphasort);
    } else {
        n = scandir(command_line->arguments[1], &filelist, NULL, alphasort);
    }

    if(n) {
        while(n--) {
            printf("%s\n", filelist[n]->d_name);
            free(filelist[n]);
        }
    }
}

void handle_cd(CommandLine* command_line) {
    // NOTE: Não tratamos o caso 'cd ~', ou seja, não fazemos a expansão de til.
    char* destination_path;
    if(command_line->argument_count == 1) {
        // Vamos para a HOME do usuário ao encontrar somente "cd" na linha de comando
        destination_path = getenv("HOME");
    } else if(command_line->argument_count > 2) {
        // 'cd' não aceita mais de um argumento
        printf("mabshell: cd : número excessivo de argumentos :(\n");
        return;
    } else {
        destination_path = command_line->arguments[1];
    }

    int status = chdir(destination_path);
    if(status < 0) {
        perror("mabshell: cd");
    }
}

void handle_jobs(CommandLine* command_line) {
    JobListNode* job_ptr = job_list.first;

    while (job_ptr != NULL) {
        print_job(job_ptr->job);
        job_ptr = job_ptr->next;
    }
    
}

pid_t handle_external_command(CommandLine* command_line) {
    return spawn_process(command_line->argument_count, command_line->arguments);
}