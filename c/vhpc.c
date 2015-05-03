#include "executer.h"
#include "organizer.h"
#include "utils.h"

int main(int argc, char *argv[]) {

  executer_init();
  organizer_init();
  start_vm("tptest");
  teleport("tptest", "172.17.10.123");

  _exit(0);
}
