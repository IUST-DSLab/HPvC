#include "executer.h"
#include "utils.h"

int main(int argc, char *argv[]) {

  executer_init();
  start_vm("Fedora");

  _exit(0);
}
