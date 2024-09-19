// Name: Hung Tran NetID: U00526311
// Desc: expose the function to execute a command
#ifndef COMMAND_H
#define COMMAND_H

#include <stdlib.h>
#define BUFFER_SIZE 255
pid_t exec_command(char **args, int num_args);

#endif
