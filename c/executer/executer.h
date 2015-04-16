#ifndef EXECUTER_H
#define EXECUTER_H

#include "VBoxCAPIGlue.h"

// Private
void _find_machine(char *vmname, IMachine **machine);
void _start_vm(IMachine *machine);

// Public
void executer_init();
void executer_onexit();
void start_vm(char *name);

#endif
