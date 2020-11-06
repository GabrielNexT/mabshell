#ifndef MABSHELL_H
#define MABSHELL_H

#include <stdbool.h>
#include <sys/types.h>

typedef struct {
    bool run_on_background;

    int argument_count;
    char** arguments;
} CommandLine;

typedef enum {
    NO_BUILTIN_COMMAND,
    FG,
    BG,
    CD,
    JOBS
} BuiltinCommand;

typedef void (*BuiltinCommandFunction)(CommandLine*);

char* read_line();

CommandLine parse_command_line(char*);

BuiltinCommand try_get_builtin_command(CommandLine*);

BuiltinCommandFunction get_builtin_command_function(BuiltinCommand);

void handle_fg(CommandLine*);

void handle_bg(CommandLine*);

void handle_cd(CommandLine*);

void handle_jobs(CommandLine*);

pid_t handle_external_command(CommandLine*);

#endif