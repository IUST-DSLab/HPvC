#ifndef EXECUTER_H
#define EXECUTER_H

#include <czmq.h>
#include "VBoxCAPIGlue.h"



typedef enum {vmstate_up, vmstate_teleported} vmstate;

struct VMMetadata {
  char *name;
  char *home;
  int n_history;
  char **history;
  ISession *session;
  vmstate state;
  bool guest;
};

// Private
void comm_actor(zsock_t *pipe, void *args);
void _find_machine(char *vmname, IMachine **machine);
void _start_vm(IMachine *machine, ISession *sess);
void add_metadata(struct VMMetadata vm_md);
char *get_host_ip(char *ip);

void log_err(char *err);
void _exit(int status);
void err_exit(char *err);

void start_vm(char *name);


#endif
