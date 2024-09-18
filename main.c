#define _GNU_SOURCE
#include "builtin.h"
#include "error.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define WHITE_SPACE " \t\r\n\a"
#define BUFFER_SIZE 1024 // TODO: is this a reasonable size?

struct proc {
  pid_t pid;
  char **args;
  int num_args;
};

char **get_tokens(char *string, int *num_tokens) {
  char **tokens = malloc(BUFFER_SIZE * sizeof(char *));
  if (tokens == NULL) {
    // fprintf(stderr, "Input buffer memory allocation failed\n");
    raise_error();
    exit(1);
  }
  int current_buffer_size = BUFFER_SIZE;
  int index = 0;
  char *token = NULL;
  while ((token = strsep(&string, WHITE_SPACE)) != NULL) {
    if (strlen(token) == 0) {
      continue;
    }
    tokens[index] = token;
    index++;
    if (index >= current_buffer_size) {
      current_buffer_size += BUFFER_SIZE;
      tokens = realloc(tokens, current_buffer_size * sizeof(char *));
    }
  }
  if (index == 0) {
    free(tokens);
    return NULL;
  }
  *num_tokens = index;
  return tokens;
}

int search_command_path(char **args) {
  char *path = strdup(getenv("PATH"));
  if (path == NULL) {
    // fprintf(stderr, "PATH environment variable not set\n");
    return 0;
  }
  char *path_ptr = path; // to avoid modifying path
  char *path_token = NULL;
  while ((path_token = strsep(&path_ptr, ":")) != NULL) {
    if (strlen(path_token) == 0) {
      continue;
    }
    char command_path[strlen(path_token) + strlen(args[0]) + 2];
    strcpy(command_path, path_token);
    strcat(command_path, "/");
    strcat(command_path, args[0]);
    if (access(command_path, X_OK) == 0) {
      // args[0] = command_path;
      // printf("Found command at %s\n", command_path);
      return 1;
    }
  }
  free(path);
  return 0;
  // fprintf(stderr, "Command not found in PATH\n");
}

// TODO: is this the best way?
int is_path_command(char *command) { return strchr(command, '/') != NULL; }

// return 1 if redirection is handled successfully, 0 otherwise
int handle_redirect(char **args, int num_args, char **redirect_filename) {
  int redirect_index = -1;
  for (int i = 0; i < num_args - 1; i++) {
    if (strcmp(args[i], ">") == 0) {
      if (redirect_index != -1) {
        // fprintf(stderr, "Syntax error: Multiple redirections\n");
        raise_error();
        // exit(1);
        return 0;
      }
      redirect_index = i;
    }
  }

  if (redirect_index != -1) {
    // printf("Redirect index: %d. Num args: %d\n", redirect_index, num_args);

    // there must be a filename after '>' and no extra args
    // and the filename must not be the first or last arg
    if (redirect_index == 0 || redirect_index != num_args - 3) {
      // fprintf(stderr, "Syntax error: Invalid redirection format.\n");
      raise_error();
      // exit(1);
      return 0;
    }

    *redirect_filename = args[redirect_index + 1];

    // remove all tokens starting from the redirection index
    args[redirect_index] = NULL;
  } else {
    *redirect_filename = NULL;
  }

  return 1;
}

pid_t exec_external_command(char **args, int num_args) {
  char *redirect_filename = NULL;
  if (!handle_redirect(args, num_args, &redirect_filename)) {
    return -1;
  }

  pid_t pid = fork();
  if (pid < 0) {
    raise_error();
  } else if (pid == 0) {
    if (redirect_filename != NULL) {
      // redirect output to file
      int fd = open(redirect_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd == -1) {
        raise_error();
        exit(1);
      }
      if (dup2(fd, STDOUT_FILENO) == -1) {
        raise_error();
        close(fd);
        exit(1);
      }
      close(fd);
    }

    if (is_path_command(args[0])) {
      if (execv(args[0], args) == -1) {
        // fprintf(stderr, "Error: %s\n", args[0]);
        raise_error();
      }
    } else if (search_command_path(args)) {
      if (execvp(args[0], args) == -1) {
        // fprintf(stderr, "Error: %s\n", args[0]);
        raise_error();
      }
    } else {
      raise_error();
    }
    exit(1);
  }

  return pid;
}

pid_t exec_command(char **args, int num_args) {
  // printf("Executing command: ");
  // for (int i = 0; i < num_args; i++) {
  //   printf("%s ", args[i]);
  // }
  // printf("\n");
  if (exec_builtin_command(args, num_args)) {
    return -1;
  }
  return exec_external_command(args, num_args);
}

int main(int argc, char *argv[]) {
  if (argc > 1) {
    // fprintf(stderr, "Usage: %s\n", argv[0]);
    raise_error();
    exit(1);
  }

  char *shell_name = "rush";
  char *string = NULL;
  size_t buffer_size = 0;
  ssize_t read_size;

  // printf("Welcome to rush shell!\n");
  setenv("PATH", "/bin", 1);

  while (1) {
    printf("%s> ", shell_name);
    fflush(stdout);

    read_size = getline(&string, &buffer_size, stdin);
    if (read_size == -1) {
      // fprintf(stderr, "Error reading input\n");
      raise_error();
      break;
    }

    int num_tokens;
    char **tokens = get_tokens(string, &num_tokens);

    int num_procs = 0;
    // TODO: should it be num_tokens / 2?
    struct proc procs[num_tokens / 2 + 1];

    if (tokens != NULL) {
      int valid_command = 1;
      int arg_start_idx = 0;
      int arg_end_idx = 0;

      for (arg_end_idx = 0; arg_end_idx < num_tokens; arg_end_idx++) {
        if (strcmp(tokens[arg_end_idx], "&") == 0) {
          // printf("Found & at %d\n", arg_end_idx);
          int num_args = arg_end_idx - arg_start_idx;

          if (num_args == 0) {
            // fprintf(stderr, "Syntax error: Empty command\n");
            raise_error();
            valid_command = 0;
            break;
          }

          // printf("collecting command\n");
          char **args = malloc((num_args + 1) * sizeof(char *));
          for (int i = 0; i < num_args; i++) {
            args[i] = tokens[arg_start_idx + i];
          }
          args[num_args] = NULL;

          procs[num_procs].args = args;
          procs[num_procs].num_args = num_args + 1;
          num_procs++;

          arg_start_idx = arg_end_idx + 1;
        }
      }
      // handle the last command
      if (valid_command && arg_end_idx == num_tokens) {
        // printf("Found last command\n");
        int num_args = arg_end_idx - arg_start_idx;

        if (num_args > 0) {
          // printf("collecting command\n");
          char **args = malloc((num_args + 1) * sizeof(char *));
          for (int i = 0; i < num_args; i++) {
            args[i] = tokens[arg_start_idx + i];
          }
          args[num_args] = NULL;

          procs[num_procs].args = args;
          procs[num_procs].num_args = num_args + 1;
          num_procs++;
        }
      }

      if (!valid_command) {
        // printf("Invalid command. Freeing args and tokens\n");
        for (int i = 0; i < num_procs; i++) {
          free(procs[i].args);
        }
        free(tokens);
        continue;
      }

      // printf("Num procs: %d\n", num_procs);
      for (int i = 0; i < num_procs; i++) {
        pid_t pid = exec_command(procs[i].args, procs[i].num_args);
        procs[i].pid = pid;
      }

      for (int i = 0; i < num_procs; i++) {
        pid_t pid = procs[i].pid;
        if (pid == -1) {
          continue;
        }
        int status = waitpid(pid, NULL, 0);
        if (status == -1) {
          // fprintf(stderr, "Error %d (%s)\n", procs[i].pid, procs[i].args[0]);
          raise_error();
          continue;
        } else {
          // printf("PID %d exited with status %d\n", pids[i], status);
        }
      }
    }

    for (int i = 0; i < num_procs; i++) {
      free(procs[i].args);
    }
    free(tokens);
  }

  if (string != NULL) {
    free(string);
  }
  return 0;
}
