#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <czmq.h>
#include "VBoxCAPIGlue.h"

#include "executer.h"
#include "utils.h"
#include "oe.pb-c.h"



IVirtualBoxClient *vboxclient = NULL;
IVirtualBox *vbox = NULL;
ISession *session = NULL;
ISession *tp_session = NULL;

zactor_t *comm;

int main(int argc, char *argv[]) {
  printf("Executer started.");
  return 0;
}

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

void _start_vm(IMachine *machine, ISession *sess) {
    IProgress *progress;
    BSTR type, env;
    HRESULT rc;
    g_pVBoxFuncs->pfnUtf8ToUtf16("gui", &type);
    rc = IMachine_LaunchVMProcess(machine, sess, type, env, &progress);
    if (FAILED(rc)) {
      err_exit("Failed to start vm.");
    }
    g_pVBoxFuncs->pfnUtf16Free(type);

    IProgress_WaitForCompletion(progress, -1);
}

void comm_actor(zsock_t *pipe, void *args) {
  zsock_signal(pipe, 0);

  HRESULT rc;
  zsock_t *listener = zsock_new_rep("tcp://127.0.0.1:23432");
  zsock_t *sender;
  zpoller_t *poller = zpoller_new(pipe, listener, NULL);
  if (!poller) err_exit("Poller was not created.\n");

  zsock_t *which = zpoller_wait(poller, -1);
  if (!zpoller_terminated(poller)) {
    if (which == listener) {
      char *smsg = zstr_recv(which);
      // Target teleport init
      if (*smsg == '1') {
        IMachine *tp_machine;
        _find_machine("tp-tptest", &tp_machine);
        rc = IVirtualBoxClient_get_Session(vboxclient, &tp_session);
        if (FAILED(rc) || !tp_session) {
          err_exit("Could not get Session reference for teleport.");
        }
        IMachine_LockMachine(tp_machine, tp_session, LockType_Write);
        ISession_get_Machine(tp_session, &tp_machine);
        IMachine_SetTeleporterEnabled(tp_machine, true);
        BSTR hostnameu;
        g_pVBoxFuncs->pfnUtf8ToUtf16("172.17.10.123", &hostnameu);
        IMachine_SetTeleporterAddress(tp_machine, hostnameu);
        g_pVBoxFuncs->pfnUtf16Free(hostnameu);
        IMachine_SetTeleporterPort(tp_machine, 87678);
        /*IConsole_SetUseHostClipboard(true);*/
        IMachine_SaveSettings(tp_machine);
        ISession_UnlockMachine(tp_session);

        ISession_Release(session);
        session = NULL;
        IMachine_Release(tp_machine);
        tp_machine = NULL;

        rc = IVirtualBoxClient_get_Session(vboxclient, &tp_session);
        if (FAILED(rc) || !tp_session) {
          err_exit("Could not get Session reference for teleport.");
        }
        _find_machine("tp_tptest", &tp_machine);
        _start_vm(tp_machine, tp_session);

        zstr_send(listener, "2");
      } else if (*smsg == '2') {
        // Source do teleport
        IConsole *console;
        ISession_get_Console(session, &console);

        BSTR hostnameu, passu;
        g_pVBoxFuncs->pfnUtf8ToUtf16("172.17.10.103", &hostnameu);
        /*g_pVBoxFuncs->pfnUtf8ToUtf16("123", &passu);*/
        rc = IConsole_Teleport(console, hostnameu, 87678, NULL, 2000, NULL);
        g_pVBoxFuncs->pfnUtf16Free(hostnameu);
      }
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
          char addr[50];
          printf("Migration begin: %s\n", ip);
          sprintf(addr, "tcp://%s:%d", ip, 23432);
          sender = zsock_new_req(addr);
          zstr_send(sender, "1");
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

  comm = zactor_new(comm_actor, NULL);
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
  zactor_destroy(&comm);

  g_pVBoxFuncs->pfnClientUninitialize();
  VBoxCGlueTerm();
}

void start_vm(char *name) {
  // Find machine
  IMachine *machine;
  _find_machine(name, &machine);
  _start_vm(machine, session);
}

void teleport(char *name, char *target) {
  char msg[50];
  sprintf(msg, "1%s", target);
  zstr_send(comm, msg);
  char *st = zstr_recv(comm);
  printf("%s\n", st);
  free(st);
}
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
