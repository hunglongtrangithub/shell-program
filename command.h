#ifndef COMMAND_H
#define COMMAND_H

#include <stdlib.h>
#define BUFFER_SIZE 255
int exec_builtin_command(char *args[], int num_args);
pid_t exec_command(char **args, int num_args);

#endif
