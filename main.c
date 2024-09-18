#include "builtin.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WHITE_SPACE " \t\r\n\a"
#define BUFFER_SIZE 1024 // TODO: is this a reasonable size?

char **get_tokens(char *string, int *num_tokens) {
  char **tokens = malloc(BUFFER_SIZE * sizeof(char *));
  if (tokens == NULL) {
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
  if (path == NULL) {
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
}

// TODO: is this the best way?
int is_path_command(char *command) { return strchr(command, '/') != NULL; }

void exec_command(char *args[], int num_args) {
  if (!exec_builtin_command(args, num_args)) {
    pid_t pid = fork();
    if (pid < 0) {
      raise_error();
      exit(1);
    } else if (pid == 0) {
      if (!is_path_command(args[0])) {
        search_command_path(args);
      }
      if (execv(args[0], args) == -1) {
        perror(args[0]);
        raise_error();
      }
      exit(1);
    } else {
      int status;
      waitpid(pid, &status, 0);
      // printf("Child process exited with status %d\n", status);
    }
  }
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
    if (tokens != NULL) {
      int num_args = 0;
      for (int i = 0; i <= num_tokens; i++) {
        if (i < num_tokens && strcmp(tokens[i], "&") != 0) {
          num_args++;
          continue;
        }
        // only reach here if tokens[i] == "&" or i == num_tokens
        if (num_args == 0) {
          // printf("& must be preceded by a command\n");
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

        exec_command(args, num_args);

        num_args = 0;
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
