#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <czmq.h>
#include "VBoxCAPIGlue.h"

#include "executer.h"
#include "utils.h"



IVirtualBoxClient *vboxclient = NULL;
IVirtualBox *vbox = NULL;
ISession *session = NULL;

void _find_machine(char *vmname, IMachine **machine) {
  HRESULT rc;
  BSTR vmname_utf16;
  g_pVBoxFuncs->pfnUtf8ToUtf16(vmname, &vmname_utf16);
  rc = IVirtualBox_FindMachine(vbox, vmname_utf16, machine);
  if (FAILED(rc) | !machine) {
    err_exit("Could not find machine.");
  }
  g_pVBoxFuncs->pfnUtf16Free(vmname_utf16);
}

void _start_vm(IMachine *machine) {
    IProgress *progress;
    BSTR type, env;
    HRESULT rc;
    g_pVBoxFuncs->pfnUtf8ToUtf16("gui", &type);
    rc = IMachine_LaunchVMProcess(machine, session, type, env, &progress);
    if (FAILED(rc)) {
      err_exit("Failed to start vm.");
    }
    g_pVBoxFuncs->pfnUtf16Free(type);

    IProgress_WaitForCompletion(progress, -1);
}

void comm_actor(zsock_t *pipe, void *args) {
  zsock_signal(pipe, 0);

  zsock_t *listener = zsock_new_rep("tcp://127.0.0.1:23432");
  zsock_t *sender;
  zpoller_t *poller = zpoller_new(pipe, listener, NULL);
  if (!poller) err_exit("Poller was not created.\n");

  zsock_t *which = zpoller_wait(poller, -1);
  if (!zpoller_terminated(poller)) {
    if (which == listener) {
      char *smsg = zstr_recv(which);
      printf("%s\n", smsg);
      free(smsg);
    } else {
      bool terminated = false;
      while (!terminated) {
        zmsg_t *msg = zmsg_recv(pipe);
        if (!msg)
            break; //  Interrupted
        char *smsg = zmsg_popstr(msg);
        //  All actors must handle $TERM in this way
        if (streq(smsg, "$TERM"))
            terminated = true;

        // Migration begin
        if (*smsg == '1') {
          char *ip = smsg + 1;
          printf("Migration begin: %s\n", ip);
          zstr_send(pipe, "ok");
        }
        printf("%s\n", smsg);

        free(smsg);
        zmsg_destroy(&msg);
      }
    }
  }

  zpoller_destroy(&poller);
  zsock_destroy(&listener);
  printf("%s\n", "Actor finished.");
}

void executer_init() {
  ULONG revision = 0;
  HRESULT rc;

  // Initialize objects
  if (VBoxCGlueInit()) {
    err_exit(g_szVBoxErrMsg);
  }

  g_pVBoxFuncs->pfnClientInitialize(NULL, &vboxclient);
  if (!vboxclient) {
    err_exit("Failed to initialize client.");
  }
  rc = IVirtualBoxClient_get_VirtualBox(vboxclient, &vbox);
  if (FAILED(rc) || !vbox) {
    err_exit("Could not get VirtualBox reference");
  }
  rc = IVirtualBoxClient_get_Session(vboxclient, &session);
  if (FAILED(rc) || !session) {
    err_exit("Could not get Session reference");
  }

  zactor_t *comm = zactor_new(comm_actor, NULL);

  zstr_send(comm, "1172.17.10.122");
  char *st = zstr_recv(comm);
  printf("%s\n", st);
  free(st);
  zactor_destroy(&comm);

}

void executer_onexit() {
  if (session) {
    ISession_Release(session);
    session = NULL;
  }
  if (vbox) {
    IVirtualBox_Release(vbox);
    vbox = NULL;
  }
  if (vboxclient) {
    IVirtualBoxClient_Release(vboxclient);
    vboxclient = NULL;
  }

  g_pVBoxFuncs->pfnClientUninitialize();
  VBoxCGlueTerm();
}

void start_vm(char *name) {
  // Find machine
  IMachine *machine;
  _find_machine(name, &machine);
  _start_vm(machine);
}

/*int main(int argc, char *argv[]) {*/
    /*// Start Machine*/
    /*[>start_vm(machine);<]*/
    /*IProgress *progress;*/
    /*BSTR type, env;*/
    /*g_pVBoxFuncs->pfnUtf8ToUtf16("gui", &type);*/
    /*rc = IMachine_LaunchVMProcess(machine, session, type, env, &progress);*/
    /*g_pVBoxFuncs->pfnUtf16Free(type);*/
    /*sleep(20);*/

    /*// Get console for machine*/
    /*IConsole *console;*/
    /*ISession_get_Console(session, &console);*/


    /*// Teleport*/
/*[>    BSTR hostnameu, passu;<]*/
    /*[>g_pVBoxFuncs->pfnUtf8ToUtf16("192.168.1.102", &hostnameu);<]*/
    /*[>g_pVBoxFuncs->pfnUtf8ToUtf16("123", &passu);<]*/
    /*[>rc = IConsole_Teleport(console, hostnameu, 4884, passu, 500);<]*/
    /*[>g_pVBoxFuncs->pfnUtf16Free(hostnameu);<]*/

    /*_exit(0);*/
/*}*/
