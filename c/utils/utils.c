#include <stdio.h>

#include "executer.h"



void log_err(char *err) {
  fprintf(stderr, err);
}

void _exit(int status) {
  executer_onexit();
  exit(status);
}

void err_exit(char *err) {
  log_err(err);
  _exit(1);
}
