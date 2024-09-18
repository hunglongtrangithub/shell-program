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

// struct proc {
//   pid_t pid;
//   char **args;
// };

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

int search_command_path(char *args[]) {
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

pid_t exec_external_command(char *args[], int num_args) {
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

    int num_pids = 0;
    // TODO: should it be num_tokens / 2?
    pid_t pids[num_tokens];
    // struct proc procs[num_tokens];

    if (tokens != NULL) {
      int num_args = 0;
      for (int i = 0; i <= num_tokens; i++) {
        if (i < num_tokens && strcmp(tokens[i], "&") != 0) {
          num_args++;
          continue;
        }
        // only reach here if tokens[i] == "&" or i == num_tokens
        if (num_args == 0) {
          // fprintf(stderr, "& must be preceded by a command\n");
          // raise_error();
          break;
        }
        char *args[num_args + 1];
        for (int j = 0; j < num_args; j++) {
          args[j] = tokens[i - num_args + j];
        }
        args[num_args] = NULL;

        // for (int j = 0; j < num_args; j++) {
        //   printf("%s ", args[j]);
        // }
        // printf("\n");

        num_args++; // account for NULL terminator
        if (!exec_builtin_command(args, num_args)) {
          pid_t pid = exec_external_command(args, num_args);
          if (pid == -1) {
            continue;
          }
          // printf("PID: %d\n", pid);
          pids[num_pids] = pid;
          // procs[num_pids].args = args;
          // procs[num_pids].pid = pid;
          num_pids++;
        }

        // reset for next command
        num_args = 0;
      }

      for (int i = 0; i < num_pids; i++) {
        pid_t pid = pids[i];
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

    free(tokens);
    // printf("\n");
  }

  if (string != NULL) {
    free(string);
  }
  return 0;
}
