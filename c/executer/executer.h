#ifndef EXECUTER_H
#define EXECUTER_H

#include <czmq.h>
#include "VBoxCAPIGlue.h"

// Private
void _find_machine(char *vmname, IMachine **machine);
void _start_vm(IMachine *machine, ISession *sess);
char *get_host_ip(char *ip);
void comm_actor(zsock_t *pipe, void *args);

void log_err(char *err);
void _exit(int status);
void err_exit(char *err);

void start_vm(char *name);
void teleport(char *name, char *target);


#endif
