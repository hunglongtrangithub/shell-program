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

void search_command_path(char *args[]) {
  char *path = getenv("PATH");
  // printf("%s: %s\n", "PATH", path);
  if (path == NULL) {
    // fprintf(stderr, "PATH environment variable not set\n");
    raise_error();
    return;
  }
  char *path_token = NULL;
  while ((path_token = strsep(&path, ":")) != NULL) {
    if (strlen(path_token) == 0) {
      continue;
    }
    char command_path[strlen(path_token) + strlen(args[0]) + 2];
    strcpy(command_path, path_token);
    strcat(command_path, "/");
    strcat(command_path, args[0]);
    if (access(command_path, X_OK) == 0) {
      args[0] = command_path;
      // printf("Found command at %s\n", command_path);
      return;
    }
  }
  // fprintf(stderr, "Command not found in PATH\n");
}

// TODO: is this the best way?
int is_path_command(char *command) { return strchr(command, '/') != NULL; }

void handle_redirect(char **args, int num_args) {
  int redirect_index = -1;
  for (int i = 0; i < num_args - 1; i++) {
    if (strcmp(args[i], ">") == 0) {
      redirect_index = i;
      break;
    }
  }

  if (redirect_index != -1) {
    // printf("Redirect index: %d. Num args: %d\n", redirect_index, num_args);

    // there must be a filename after '>' and no extra args
    if (redirect_index != num_args - 3) {
      fprintf(stderr, "Syntax error: Invalid redirection format.\n");
      raise_error();
      exit(1);
    }

    // open the file for writing (clear content if exists, create if not)
    // printf("Redirecting stdout to %s\n", args[redirect_index + 1]);
    int fd = open(args[redirect_index + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
      raise_error();
      exit(1);
    }

    // redirect
    if (dup2(fd, STDOUT_FILENO) == -1) {
      raise_error();
      close(fd);
      exit(1);
    }

    close(fd);

    // remove all tokens starting from the redirection index
    args[redirect_index] = NULL;
  }
}

pid_t exec_ext_command(char *args[], int num_args) {
  pid_t pid = fork();
  if (pid < 0) {
    // printf("Fork failed\n");
    raise_error();
    exit(1);
  } else if (pid == 0) {
    handle_redirect(args, num_args);
    if (!is_path_command(args[0])) {
      search_command_path(args);
    }
    if (execv(args[0], args) == -1) {
      perror(args[0]);
      raise_error();
    }
    exit(1);
  }
  return pid;
}

int main(int argc, char *argv[]) {
  if (argc > 1) {
    raise_error();
    exit(1);
  }

  char *shell_name = "rush";
  char *string = NULL;
  size_t buffer_size = 0;
  ssize_t read_size;

  // printf("Welcome to rush shell!\n");

  while (1) {
    printf("%s> ", shell_name);
    fflush(stdout);

    read_size = getline(&string, &buffer_size, stdin);
    if (read_size == -1) {
      raise_error();
      break;
    }

    int num_tokens;
    char **tokens = get_tokens(string, &num_tokens);

    int num_pids = 0;
    // TODO: should it be num_tokens / 2?
    pid_t pids[num_tokens];

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
          raise_error();
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
          pid_t pid = exec_ext_command(args, num_args);
          // printf("PID: %d\n", pid);
          pids[num_pids] = pid;
          num_pids++;
        }

        num_args = 0;
      }

      for (int i = 0; i < num_pids; i++) {
        int status = waitpid(pids[i], NULL, 0);
        if (status == -1) {
          raise_error();
          continue;
        } else {
          /*printf("PID %d exited with status %d\n", pids[i], status);*/
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
