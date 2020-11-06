#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>

pid_t spawn_process(int argument_count, char** arguments) {
    pid_t pid = fork();

    if(pid < 0) {
        // Erro ao executar fork.
        perror("mabshell: fail fork() :");
    } else if(pid == 0) {
        // Processo filho
        int status = execv(arguments[0], arguments);
        if(status < 0) {
            perror("mabshell: fail execv :");
            exit(1);
        }
    }

    return pid;
}