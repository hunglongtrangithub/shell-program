#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char *builtin_commands[] = {"cd", "exit", "path"};
typedef int (*builtin_func)(char **args, int num_args);
struct builtin_command {
  char *name;
  builtin_func func;
};

int builtin_path(char **args, int num_args) {
  int len_new_path = 0;
  for (int i = 1; i < num_args - 1; i++) {
    // TODO: should I check if the path exists?
    // +1 for colon or null terminator for the last arg
    len_new_path += strlen(args[i]) + 1;
  }

  char new_path[len_new_path];

  // set length of new_path to 0
  new_path[0] = '\0';
  for (int i = 1; i < num_args - 1; i++) {
    strcat(new_path, args[i]);
    strcat(new_path, ":");
  }
  new_path[len_new_path - 1] = '\0';
  printf("%s: %s\n", "new PATH", new_path);
  setenv("PATH", new_path, 1);
  return 1;
}

int builtin_exit(char **args, int num_args) {
  if (num_args != 2) {
    raise_error();
    return 1;
  }
  exit(0);
}

int builtin_cd(char **args, int num_args) {
  if (num_args != 3) {
    raise_error();
    return 1;
  }
  if (chdir(args[1]) != 0) {
    raise_error();
  }
  return 1;
}

const struct builtin_command builtins[] = {
    {"cd", builtin_cd}, {"exit", builtin_exit}, {"path", builtin_path}};
int num_builtins() { return sizeof(builtins) / sizeof(struct builtin_command); }

int exec_builtin_command(char *args[], int num_args) {
  if (args[0] == NULL) {
    return 1;
  }
  for (int i = 0; i < num_builtins(); i++) {
    if (strcmp(args[0], builtins[i].name) == 0) {
      return builtins[i].func(args, num_args);
    }
  }
  return 0;
}
