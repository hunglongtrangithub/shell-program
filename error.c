#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const char error_message[] = "An error has occurred\n";
void raise_error() {
  write(STDERR_FILENO, error_message, strlen(error_message));
  fflush(stderr);
}
