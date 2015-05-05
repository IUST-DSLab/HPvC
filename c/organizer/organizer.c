#include <stdio.h>
#include <stdlib.h>

/*#include "executer.h"*/
#include <czmq.h>
#include "oe.pb-c.h"


int main(int argc, char *argv[]) {
  printf("Organizer started.");
  zsock_t *executer_sock = zsock_new_req("tcp://127.0.0.1:98789");

  Teleport tpm = TELEPORT__INIT;
  void *buf;
  unsigned len;

  tpm.vm_name = "tptest";
  tpm.target_ip = "172.17.10.123";
  len = teleport__get_packed_size(&tpm);

  buf = malloc(len);
  teleport__pack(&tpm, buf);

  /*printf(buf);*/

  zstr_send(executer_sock, buf);
  sleep(2);
  zsock_destroy(&executer_sock);
  return 0;
}
