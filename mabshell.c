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
JobList job_list;

int main(int argc, char** argv) {

    job_list = new_job_list();
    
    signal(SIGINT, handle_sig_int);
    signal(SIGTSTP, handle_sig_stop);

    // Utilizando SigAction para ouvir SIGCHLD uma vez que só assim podemos saber o PID do processo filho que originou o sinal
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handle_sig_child; 
    sigaction(SIGCHLD, &sa, NULL);

    while(true) {
        // Escrevendo prompt
        printf("\033[1;32mmabshell\n> \033[0m");
        // Lendo comando
        char* line = read_line();

        // Parse da linha de comando
        CommandLine command_line = parse_command_line(line);

        if(command_line.argument_count != 0) {
            BuiltinCommand builtin = try_get_builtin_command(&command_line);
            if(builtin != NO_BUILTIN_COMMAND) {
                BuiltinCommandFunction command_function = get_builtin_command_function(builtin);
                command_function(&command_line);

            } else {
                pid_t pid = handle_external_command(&command_line);
                if(pid > 0) {
                    add_process_to_job_list(&job_list, pid, RUNNING, line);

                    // Processo criado com sucesso
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

                if(WIFEXITED(status)) {
                    update_job_list(&job_list, foreground_process_id, EXITED);
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
        
        if(fgets(write_ptr, buffer_size - offset, stdin) == NULL) {
            if(feof(stdin)) {
                printf("\nExit: End-of-file\n");
                exit(0);
            } else {
                offset = 0;
                continue;
            }
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
    default:
        return NULL;
    }
}

void handle_sig_int(int signal) {
    if(has_foreground_process) {
        kill(-foreground_process_id, SIGINT);
        has_foreground_process = false;
    }
}

void handle_sig_stop(int signal) {
    if(has_foreground_process) {
        kill(-foreground_process_id, SIGTSTP);
        has_foreground_process = false;
    }
}

void handle_sig_child(int signal, siginfo_t *si, void *ucontext) {
    int status;
    
    pid_t pid = waitpid(si->si_pid, &status, WNOHANG | WUNTRACED);

    // Sinal foi recebido, mas wait não retornou nada. Processo foi parado.
    if(pid == 0) {
        update_job_list(&job_list, si->si_pid, STOPPED);
        return;
    }

    // Processo terminou
    if(WIFEXITED(status)) {
        update_job_list(&job_list, si->si_pid, EXITED);
        return;
    }
    
    // Recebemos um sinal que diz que o filho parou
    if(WIFSIGNALED(status) && WTERMSIG(status) == SIGTSTP) {
        update_job_list(&job_list, si->si_pid, STOPPED);
        return;
    }
}


void job_to_background(Job job) {
    if(job.status == EXITED) {
        printf("mabshell: bg : %d : trabalho já finalizado\n", job.process_id);
    }

    // TODO: Rodar apenas de processo estiver parado
    kill(job.process_id, SIGCONT);
    update_job_list(&job_list, job.process_id, RUNNING);

    puts(job.line);
}

void job_to_foreground(Job job) {
    if(job.status == EXITED) {
        printf("mabshell: fg : %d : trabalho já finalizado\n", job.process_id);
    }

    // TODO: Rodar apenas de processo estiver parado
    kill(job.process_id, SIGCONT);
    update_job_list(&job_list, job.process_id, RUNNING);
    
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
        // Utilizando um JID
        int jid = atoi(arg + 1);
        if(!get_job_with_jid(&job_list, jid, &job)) {
            printf("mabshell: fg : %s : trabalho não existe\n", arg);
            return;
        }
    } else {
        // Utilizando um PID
        pid_t pid = atoi(arg);
        if(!get_job_with_pid(&job_list, pid, &job)) {
            printf("mabshell: fg : %s : trabalho não existe\n", arg);
            return;
        }
    }

    job_to_foreground(job);
}

void handle_bg(CommandLine* command_line) {
    if(command_line->argument_count == 1) {
        if(job_list.last == NULL) {
            printf("mabshell: bg : atual : trabalho não existe\n");
        } else {
            job_to_background(job_list.last->job);
        }

        return;
    }

    char* arg = command_line->arguments[1];

    Job job;
    if(arg[0] == '%') {
        // Utilizando um JID
        int jid = atoi(arg + 1);
        if(!get_job_with_jid(&job_list, jid, &job)) {
            printf("mabshell: bg : %s : trabalho não existe\n", arg);
            return;
        }
    } else {
        // Utilizando um PID
        pid_t pid = atoi(arg);
        if(!get_job_with_pid(&job_list, pid, &job)) {
            printf("mabshell: bg : %s : trabalho não existe\n", arg);
            return;
        }
    }

    job_to_background(job);
}

void handle_pwd(CommandLine* command_line) {
    char path[PATH_MAX];
    getcwd(path, PATH_MAX);

    printf("%s\n", path);
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
    
    // TODO: Função está dando SEG_FAULT
    // remove_exited_jobs(&job_list);
}

pid_t handle_external_command(CommandLine* command_line) {
    return spawn_process(command_line->argument_count, command_line->arguments);
}