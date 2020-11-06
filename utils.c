#include <stdlib.h>
#include <string.h>

int get_argument_count(char* line) {
    int result = 0;

    while(*line != '\0') {
        while (*line == ' ') {
            line++;
        }
        
        if(*line == '\0') {
            break;
        }

        result++;
        
        while (*line != ' ' && *line != '\0') {
            line++;
        }
    }

    return result;
}

void get_arguments(char** result, char* line) {
    int argument_index = 0;

    while(*line != '\0') {
        while (*line == ' ') {
            line++;
        }
        
        if(*line == '\0') {
            break;
        }

        char* argument_start = line;
        int argument_length = 0;
        
        while (*line != ' ' && *line != '\0') {
            argument_length++;

            line++;
        }

        result[argument_index] = (char*) malloc((argument_length + 1) * sizeof(char));
        strncpy(result[argument_index], argument_start, argument_length);
        result[argument_index][argument_length] = '\0';

        argument_index++;
    }
}