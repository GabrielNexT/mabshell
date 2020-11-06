#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <sys/types.h>

#include "mabshell.h"
#include "utils.h"

int main(int argc, char** argv) {

    while(true) {
        // Writing prompt
        printf("mabshell> ");

        // Reading a command
        char* line = read_line();

        // Parsing the command line
        CommandLine command_line = parse_command_line(line);
        free(line);

        if(command_line.argument_count != 0) {
            BuiltinCommand builtin = try_get_builtin_command(&command_line);
            if(builtin != NO_BUILTIN_COMMAND) {
                BuiltinCommandFunction command_function = get_builtin_command_function(builtin);
                command_function(&command_line);
            } else {
                handle_external_command(&command_line);
            }
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
        char* write_ptr = result + (offset * sizeof(char));
        
        if(fgets(write_ptr, buffer_size, stdin) == NULL) {
            // TODO: Fim de arquivo. Terminar execucao do codigo
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

    result.arguments = (char**) malloc(result.argument_count * sizeof(char*));

    get_arguments(result.arguments, line);

    return result;
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
    default:
        return NULL;
    }
}

void handle_fg(CommandLine* command_line) {
    printf("Handling fg\n");
}

void handle_bg(CommandLine* command_line) {
    printf("Handling bg\n");
}

void handle_cd(CommandLine* command_line) {
    printf("Handling cd\n");
}

void handle_jobs(CommandLine* command_line) {
    printf("Handling jobs\n");
}

pid_t handle_external_command(CommandLine* command_line) {
    printf("handling external command\n");
}