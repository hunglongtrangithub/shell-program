// Name: Hung Tran NetID: U00526311
// Desc: define a function to write an error message to stderr
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const char error_message[30] = "An error has occurred\n";
void raise_error() {
  write(STDERR_FILENO, error_message, strlen(error_message));
  fflush(stderr);
}
