#define _GNU_SOURCE
#include "command.h"
#include "error.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define WHITE_SPACE " \t\r\n\a"

struct proc {
  pid_t pid;
  char **args;
  int num_args;
};

int get_tokens(char *string, char **tokens) {
  int num_tokens = 0;
  char *token = NULL;
  while ((token = strsep(&string, WHITE_SPACE)) != NULL) {
    if (strlen(token) == 0) {
      continue;
    }
    tokens[num_tokens] = token;
    num_tokens++;
  }
  return num_tokens;
}

int main(int argc, char *argv[]) {
  if (argc > 1) {
    // fprintf(stderr, "Usage: %s\n", argv[0]);
    raise_error();
    exit(1);
  }

  char *shell_name = "rush";
  char *string = NULL;
  char *tokens[BUFFER_SIZE];
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

    int num_tokens = get_tokens(string, tokens);
    if (num_tokens == 0) {
      continue;
    }

    int num_procs = 0;
    // TODO: should it be num_tokens / 2?
    struct proc procs[num_tokens];

    int valid_command = 1;
    int arg_start_idx = 0;
    int arg_end_idx = 0;

    for (arg_end_idx = 0; arg_end_idx < num_tokens; arg_end_idx++) {
      if (strcmp(tokens[arg_end_idx], "&") == 0) {
        // silence error message when & is the only token
        if (arg_end_idx == 0 && num_tokens == 1) {
          // fprintf(stderr, "Syntax error: Empty command\n");
          valid_command = 0;
          break;
        }
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
      waitpid(pid, NULL, 0);
    }

    for (int i = 0; i < num_procs; i++) {
      free(procs[i].args);
    }
  }

  if (string != NULL) {
    free(string);
  }
  return 0;
}
