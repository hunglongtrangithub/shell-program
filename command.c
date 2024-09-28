// Name: Hung Tran NetID: U00526311
// Desc: bunch of functions dealing with NULL-terminated array of string
// arguments. All of the strings are heap-allocated
#define _GNU_SOURCE
#include "error.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 255

const char *builtin_commands[] = {"cd", "exit", "path"};
typedef int (*builtin_func)(char **args, int num_args);
struct builtin_command {
  char *name;
  builtin_func func;
};

// functions for the three builtin commands
// return 1 if the builtin command is executed successfully, 0 otherwise
int builtin_path(char **args, int num_args) {
  if (num_args == 2) {
    // no new paths provided
    // printf("%s: %s\n", "new PATH", "");
    setenv("PATH", "", 1);
    return 1;
  }

  char *path = getenv("PATH");
  if (path == NULL) {
    // fprintf(stderr, "PATH environment variable not set\n");
    return 0;
  }
  // avoid modifying the original path
  char *path_copy = strdup(path);

  int len_new_path = 0;
  for (int i = 1; i < num_args - 1; i++) {
    // +1 for colon or null terminator
    len_new_path += strlen(args[i]) + 1;
  }

  char new_path[len_new_path + strlen(path_copy)];

  // initialize length of new_path to 0
  new_path[0] = '\0';
  // add all new paths
  for (int i = 1; i < num_args - 1; i++) {
    // do I need to check if the added path is valid? maybe not
    strcat(new_path, args[i]);
    strcat(new_path, ":");
  }
  // add the original path at the end
  strcat(new_path, path_copy);
  new_path[len_new_path + strlen(path_copy)] = '\0';

  setenv("PATH", new_path, 1);
  free(path_copy);
  return 1;
}
int builtin_exit(char **args, int num_args) {
  if (num_args != 2) {
    return 0;
  }
  exit(0);
}
int builtin_cd(char **args, int num_args) {
  // has to have cd, directory, and NULL
  if (num_args != 3) {
    return 0;
  }
  if (chdir(args[1]) != 0) {
    return 0;
  }
  return 1;
}

const struct builtin_command builtins[] = {
    {"cd", builtin_cd}, {"exit", builtin_exit}, {"path", builtin_path}};
int num_builtins() { return sizeof(builtins) / sizeof(struct builtin_command); }

// return flag of whether the command is a builtin command
int exec_builtin_command(char *args[], int num_args) {
  if (args[0] == NULL) {
    return 1;
  }
  for (int i = 0; i < num_builtins(); i++) {
    if (strcmp(args[0], builtins[i].name) == 0) {
      if (!builtins[i].func(args, num_args)) {
        // raise error if the builtin command fails
        raise_error();
      }
      return 1;
    }
  }
  return 0;
}

int search_command_path(char *cmd_name, char *cmd_path) {
  char *path = (getenv("PATH"));
  if (path == NULL) {
    // fprintf(stderr, "PATH environment variable not set\n");
    return 0;
  }
  // avoid modifying the original path
  char *path_copy = strdup(path);

  char *path_ptr = path_copy;
  char *path_token = NULL;
  while ((path_token = strsep(&path_ptr, ":")) != NULL) {
    // consecutive colons? empty path? skip
    if (strlen(path_token) == 0) {
      continue;
    }
    // join as path_token/cmd_name
    sprintf(cmd_path, "%s/%s", path_token, cmd_name);
    if (access(cmd_path, X_OK) == 0) {
      free(path_copy);
      return 1;
    }
  }
  free(path_copy);
  // fprintf(stderr, "Command not found in PATH\n");
  return 0;
}

// return 1 if redirection is handled successfully, 0 otherwise
int handle_redirect(char **args, int num_args, char **redirect_filepath) {
  int redirect_index = -1;
  for (int i = 0; i < num_args - 1; i++) {
    if (strcmp(args[i], ">") == 0) {
      if (redirect_index != -1) {
        // fprintf(stderr, "Syntax error: Multiple redirections\n");
        return 0;
      }
      redirect_index = i;
    }
  }

  if (redirect_index != -1) {
    // there must be a filename after '>' and no extra args
    // and the filename must not be the first or last arg
    if (redirect_index == 0 || redirect_index != num_args - 3) {
      // fprintf(stderr, "Syntax error: Invalid redirection format.\n");
      return 0;
    }

    // the filename is token right next to the redirection token
    *redirect_filepath = args[redirect_index + 1];

    // remove all tokens starting from the redirection index
    args[redirect_index] = NULL;
  } else {
    *redirect_filepath = NULL;
  }

  return 1;
}

pid_t exec_external_command(char **args, int num_args) {
  char *redirect_filepath = NULL;
  if (!handle_redirect(args, num_args, &redirect_filepath)) {
    raise_error();
    return -1;
  }

  pid_t pid = fork();
  if (pid < 0) {
    // fail to fork
    // printf("Fork failed\n");
    raise_error();
    exit(1);
  } else if (pid == 0) {
    if (redirect_filepath != NULL) {
      // redirect output to file
      int fd = open(redirect_filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd == -1) {
        // fail to open file
        // fprintf(stderr, "Failed to open file\n");
        raise_error();
        exit(1);
      }
      if (dup2(fd, STDOUT_FILENO) == -1) {
        // fail to redirect output
        // fprintf(stderr, "Failed to redirect output\n");
        raise_error();
        close(fd);
        exit(1);
      }
      close(fd);
    }

    // always search PATH for the command
    char full_path[BUFFER_SIZE];
    if (!search_command_path(args[0], full_path)) {
      raise_error();
      exit(1);
    }

    // execute the command using execv
    execv(full_path, args);

    // reach here when execv fails
    exit(1);
  }

  return pid;
}

pid_t exec_command(char **args, int num_args) {
  // return -1 if the command is a builtin command
  if (exec_builtin_command(args, num_args)) {
    return -1;
  }
  return exec_external_command(args, num_args);
}
