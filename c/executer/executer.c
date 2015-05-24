#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <czmq.h>
#include "VBoxCAPIGlue.h"

#include "executer.h"
#include "utils.h"
#include "oe.pb-c.h"
#include "ee.pb-c.h"

#define TP_PASS "123"
#define TP_PORT 23632
#define TP_SOURCE "172.17.10.80"
#define TP_TARGET "172.17.8.165"



struct VMMetadata {
  char *home;
  int history_len;
  char **history;
};

IVirtualBoxClient *vboxclient = NULL;
IVirtualBox *vbox = NULL;
ISession *session = NULL;
ISession *tp_session = NULL;

struct VMMetadata *metadatas;
int metadatas_len;

zactor_t *comm;

int main(int argc, char *argv[]) {
  printf("Executer started.\n");
/*  zsock_t *organizer_sock = zsock_new_rep("tcp://127.0.0.1:98789");*/

  /*Teleport *tpm;*/
  /*char *smsg = zstr_recv(organizer_sock);*/
  /*int len = strlen(smsg);*/
  /*tpm = teleport__unpack(NULL, len, smsg);*/
  /*printf("%s %s", tpm->vm_name, tpm->target_ip);*/
  /*[>printf("%s\n", smsg);<]*/
  /*zstr_free(&smsg);*/
  /*zsock_destroy(&organizer_sock);*/


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

  metadatas_len = 0;
  metadatas = malloc(5 * sizeof(struct VMMetadata));
  comm = zactor_new(comm_actor, NULL);

  /*sleep(2);*/
  /*zstr_send(comm, "1");*/

  sleep(60);
  _exit(0);
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
  zsock_t *listener = zsock_new_rep("tcp://0.0.0.0:23432");
  zsock_t *sender;
  zpoller_t *poller = zpoller_new(pipe, listener, NULL);
  if (!poller) err_exit("Poller was not created.\n");

  bool terminated = false;
  while (!terminated) {
    zsock_t *which = zpoller_wait(poller, -1);
    if (!zpoller_terminated(poller)) {
      if (which == listener) {
        char *smsg = zstr_recv(listener);
        printf("%s\n", smsg);
        // Target teleport init
        if (*smsg == '1') {
          IMachine *tp_machine;
          _find_machine("fdr", &tp_machine);
          rc = IVirtualBoxClient_get_Session(vboxclient, &tp_session);
          if (FAILED(rc) || !tp_session) {
            err_exit("Could not get Session reference for teleport.");
          }
          IMachine_LockMachine(tp_machine, tp_session, LockType_Write);
          ISession_get_Machine(tp_session, &tp_machine);
          IMachine_SetTeleporterEnabled(tp_machine, true);
          BSTR hostnameu, passu;
          g_pVBoxFuncs->pfnUtf8ToUtf16(TP_TARGET, &hostnameu);
          g_pVBoxFuncs->pfnUtf8ToUtf16(TP_PASS, &passu);
          IMachine_SetTeleporterAddress(tp_machine, hostnameu);
          IMachine_SetTeleporterPassword(tp_machine, passu);
          g_pVBoxFuncs->pfnUtf16Free(hostnameu);
          IMachine_SetTeleporterPort(tp_machine, TP_PORT);
          /*IConsole_SetUseHostClipboard(true);*/
          IMachine_SaveSettings(tp_machine);
          ISession_UnlockMachine(tp_session);

          ISession_Release(tp_session);
          tp_session = NULL;
          IMachine_Release(tp_machine);
          tp_machine = NULL;

          rc = IVirtualBoxClient_get_Session(vboxclient, &tp_session);
          if (FAILED(rc) || !tp_session) {
            err_exit("Could not get Session reference for teleport.");
          }
          _find_machine("fdr", &tp_machine);
          sleep(1);
          /*_start_vm(tp_machine, tp_session);*/

          IProgress *tp_progress;
          BSTR type, env;
          g_pVBoxFuncs->pfnUtf8ToUtf16("gui", &type);
          rc = IMachine_LaunchVMProcess(tp_machine, tp_session, type, env, &tp_progress);
          if (FAILED(rc)) {
            err_exit("Failed to start vm.");
          }
          g_pVBoxFuncs->pfnUtf16Free(type);

          sleep(3);

          zstr_send(listener, "2");

          IProgress_WaitForCompletion(tp_progress, -1);
          ISession_UnlockMachine(tp_session);

          char *smd = zstr_recv(listener);
          TeleportMetadata *md;
          int len = strlen(smd);
          md = teleport_metadata__unpack(NULL, len, smd);
          printf("VM Metadata arrived.\n");
          printf("Home: %s\tHISTORY_LEN: %d\n", md->home, md->history_len);
          zstr_free(&smd);

          struct VMMetadata vm_md = {home: md->home, history_len: md->history_len + 1, history: md->history};
          vm_md.history = realloc(vm_md.history, (vm_md.history_len) * sizeof(char*));
          vm_md.history[vm_md.history_len - 1] = TP_TARGET;

          printf("VM MD object home: %s\thistory_len: %d\n", md->home, md->history_len);
          int i = 0;
          for (i = 0; i <= md->history_len; i++) {
            printf("%s\n", md->history[i]);
          }
        }
        zstr_free(&smsg);
      } else if (sender && (which == sender)) {
        char *smsg = zstr_recv(sender);
        if (*smsg == '2') {
          // Source do teleport
          IMachine *tp_machine;
          rc = IVirtualBoxClient_get_Session(vboxclient, &tp_session);
          if (FAILED(rc) || !tp_session) {
            err_exit("Could not get session for teleport.\n");
          }
          _find_machine("fdr", &tp_machine);
          _start_vm(tp_machine, tp_session);

          IConsole *tp_console;
          rc = ISession_get_Console(tp_session, &tp_console);
          if (FAILED(rc) || !tp_console) {
            log_err("Couldnt get console for session in teleport.\n");
          }

          IProgress *progress;
          BSTR hostnameu, passu;
          g_pVBoxFuncs->pfnUtf8ToUtf16(TP_TARGET, &hostnameu);
          g_pVBoxFuncs->pfnUtf8ToUtf16(TP_PASS, &passu);
          rc = IConsole_Teleport(tp_console, hostnameu, TP_PORT, passu, 250, &progress);
          if (FAILED(rc)) {
            log_err("Teleport Failed. ");
            printf("Retrun code %x\n", rc);
          }
          g_pVBoxFuncs->pfnUtf16Free(hostnameu);
          g_pVBoxFuncs->pfnUtf16Free(passu);

          IProgress_WaitForCompletion(progress, -1);

          TeleportMetadata md = TELEPORT_METADATA__INIT;
          void *buf;
          unsigned len;
          md.home = TP_SOURCE;
          md.history_len = 0;
          len = teleport_metadata__get_packed_size(&md);
          buf = malloc(len);
          teleport_metadata__pack(&md, buf);

          zstr_send(sender, buf);
          free(buf);
        }
        zstr_free(&smsg);
      } else {
        zmsg_t *msg = zmsg_recv(pipe);
        if (!msg)
            break; //  Interrupted
        char *smsg = zmsg_popstr(msg);
        //  All actors must handle $TERM in this way
        if (streq(smsg, "$TERM"))
            terminated = true;

        // Migration begin
        if (*smsg == '1') {
          /*char *ip = smsg + 1;*/
          /*char addr[50];*/
          /*printf("Migration begin: %s\n", ip);*/
          /*sprintf(addr, "tcp://%s:%d", ip, 23432);*/
          sender = zsock_new_req("tcp://172.17.10.80:23432");
          zstr_send(sender, "1");
          zpoller_add(poller, sender);
          /*zstr_send(pipe, "ok");*/
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
