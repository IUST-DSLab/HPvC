#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*#include "executer.h"*/
#include <czmq.h>
#include "oe.pb-c.h"


int main(int argc, char *argv[]) {
  printf("Organizer started.\n");
  zsock_t *executer_sock = zsock_new_req("tcp://127.0.0.1:98789");

  while (1) {
    char cmd[50] = {0};
    printf("What do you want? ");
    scanf("%s", &cmd);

    if (streq(cmd, "teleport")) {
      char target[50] = {0}, name[50] = {0};
      printf("Which? Where? ");
      scanf("%s %s", &name, &target);

      /*printf("Name: %s\tTarget: %s\n", name, target);*/
      OEMsg oem = OEMSG__INIT;
      Teleport tpm = TELEPORT__INIT;
      void *buf;
      unsigned len;

      tpm.vm_name = name;
      tpm.target_ip = target;
      oem.teleport = &tpm;
      oem.type = OEMSG__TYPE__TELEPORT;
      len = oemsg__get_packed_size(&oem);

      buf = malloc(len);
      oemsg__pack(&oem, buf);

      /*printf(buf);*/

      zstr_send(executer_sock, buf);
      memset(buf, 0, len * (sizeof buf[0]));
      free(buf);

      char *smsg;
      smsg = zstr_recv(executer_sock);
      free(smsg);
    } else if (streq(cmd, "exit")) {
      break;
    } else {
      break;
    }
  }

  zsock_destroy(&executer_sock);
  return 0;
}
