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
#define TP_SOURCE "172.17.10.45"
#define TP_TARGET "172.17.9.132"



IVirtualBoxClient *vboxclient = NULL;
IVirtualBox *vbox = NULL;
ISession *session = NULL;
ISession *tp_session = NULL;

struct VMMetadata *metadatas;
int metadatas_len;

char *host_ip;

zactor_t *comm;

int main(int argc, char *argv[]) {

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

  host_ip = malloc(20 * sizeof(char));
  host_ip = get_host_ip(host_ip);

  printf("Host IP: %s\n", host_ip);

  comm = zactor_new(comm_actor, NULL);

  /*sleep(2);*/
  /*zstr_send(comm, "1");*/

  zsock_t *organizer_sock = zsock_new_rep("tcp://127.0.0.1:98789");
  char *smsg;
  while (1) {
    /*smsg = zstr_recv(organizer_sock);*/
    /*char *smsg;*/
    int len;
    zframe_t *frame = zframe_recv(organizer_sock);
    len = zframe_size(frame);
    smsg = zframe_data(frame);
    /*zframe_print(frame, NULL);*/

    if (smsg == NULL) {
      break;
    }

    OEMsg *oem;
    /*int len = strlen(smsg);*/
    if (!len) {
      log_err("Message rcvd from organizer empty.\n");
    }

    /*printf("SMSG %s\n******%d\n", smsg, len);*/
    oem = oemsg__unpack(NULL, len, smsg);
    /*printf("After sending ok \s \n", oem->start->vm_name);*/
    if (oem->type == OEMSG__TYPE__TELEPORT) {
      zstr_send(organizer_sock, "ok");
      zstr_send(comm, smsg);
    } else {
      zstr_send(organizer_sock, "ok");
      char *vm_name = oem->start->vm_name;
      ISession *tmp_session;
      IMachine *tmp_machine;

      rc = IVirtualBoxClient_get_Session(vboxclient, &tmp_session);
      if (FAILED(rc) || !tmp_session) {
        err_exit("Could not get Session reference for teleport.");
      }
      _find_machine(vm_name, &tmp_machine);
      // Prevent SegFault, probably because of async works
      sleep(1);
      _start_vm(tmp_machine, tmp_session);

      struct VMMetadata vm_md = {name: vm_name, home: host_ip,
        n_history: 1, session: tmp_session, guest: false
      };

      vm_md.history = malloc(1 * sizeof(char*));
      vm_md.history[0] = host_ip;
      add_metadata(vm_md);

      free(vm_name);
    }

    /*free(smsg);*/
  }

  zsock_destroy(&organizer_sock);

  free(host_ip);
  _exit(0);
}

void comm_actor(zsock_t *pipe, void *args) {
  zsock_signal(pipe, 0);

  HRESULT rc;
  zsock_t *listener = zsock_new_rep("tcp://0.0.0.0:23432");
  zsock_t *sender;
  zpoller_t *poller = zpoller_new(pipe, listener, NULL);
  if (!poller) err_exit("Poller was not created.\n");

  char *target_ip = malloc(20 * sizeof(char));
  char *vm_name;

  bool terminated = false;
  while (!terminated) {
    zsock_t *which = zpoller_wait(poller, -1);
    if (!zpoller_terminated(poller)) {
      if (which == listener) {
        char *smsg = zstr_recv(listener);
        printf("%s\n", smsg);
        // Target teleport init
        if (*smsg == '1') {
          // Get VM name
          zsock_signal(listener, 0);
          // Not shared variable to allow for multiple incoming
          char *vm_name_l = zstr_recv(listener);

          // Make VM ready for teleportation
          IMachine *tp_machine;
          _find_machine(vm_name_l, &tp_machine);
          rc = IVirtualBoxClient_get_Session(vboxclient, &tp_session);
          if (FAILED(rc) || !tp_session) {
            err_exit("Could not get Session reference for teleport.");
          }
          IMachine_LockMachine(tp_machine, tp_session, LockType_Write);
          ISession_get_Machine(tp_session, &tp_machine);
          IMachine_SetTeleporterEnabled(tp_machine, true);
          BSTR hostnameu, passu;
          g_pVBoxFuncs->pfnUtf8ToUtf16(host_ip, &hostnameu);
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

          // Start VM and wait for source
          rc = IVirtualBoxClient_get_Session(vboxclient, &tp_session);
          if (FAILED(rc) || !tp_session) {
            err_exit("Could not get Session reference for teleport.");
          }
          _find_machine(vm_name_l, &tp_machine);
          // Prevent SegFault, probably because of async works
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

          // Inform source to commence teleporting
          zstr_send(listener, "2");

          // Wait for teleportation to end and check success
          IProgress_WaitForCompletion(tp_progress, -1);
          int rescode = -1;
          IProgress_get_ResultCode(tp_progress, &rescode);
          if (FAILED(rescode)) {
            printf("Return code %x\n", rc);
            IVirtualBoxErrorInfo *err;
            IProgress_get_ErrorInfo(tp_progress, &err);
            if (err != NULL) {
              BSTR textu;
              char *text;
              IVirtualBoxErrorInfo_get_Text(err, &textu);
              g_pVBoxFuncs->pfnUtf16ToUtf8(textu, &text);
              printf("Error: %s\n", text);
              free(text);
            }
            err_exit("Teleport Failed\n");
          }
          /*ISession_UnlockMachine(tp_session);*/

          // Get VM metadatas
          char *smd;
          int len;
          zframe_t *frame = zframe_recv(listener);
          len = zframe_size(frame);
          smd = zframe_data(frame);
          /*zframe_print(frame, NULL);*/
          if (!smd) {
            log_err("SMD is empty\n");
          }

          TeleportMetadata *md;
          /*int len = strlen(smd);*/
          md = teleport_metadata__unpack(NULL, len, smd);
          if (md == NULL) {
            log_err("MD is NULL.\n");
          }
          /*printf("VM Metadata arrived len %d.\n", len);*/
          /*printf("%s\n------------\n", smd);*/
          /*printf("Home: %s\n", md->home);*/
          /*zstr_free(&smd);*/
          /*free(smd);*/
          zframe_destroy(&frame);

          struct VMMetadata vm_md = {name: vm_name_l, home: md->home,
            n_history: md->n_history + 1, history: md->history,
            session: tp_session, guest: true
          };

          vm_md.history = realloc(vm_md.history, (vm_md.n_history) * sizeof(char*));
          vm_md.history[vm_md.n_history - 1] = TP_TARGET;

          // Allocate more space for metadatas if already filled
          if (metadatas_len % 5 == 0) {
            metadatas = realloc(metadatas, (metadatas_len + 5) * sizeof(struct VMMetadata));
          }
          metadatas[metadatas_len++] = vm_md;
          printf("VM MD object name: %s\thome: %s\thistory_len: %d\n", vm_md.name, vm_md.home, vm_md.n_history);
          int i = 0;
          for (i = 0; i <= md->n_history; i++) {
            printf("%s\n", md->history[i]);
          }
        }
        zstr_free(&smsg);
      } else if (sender && (which == sender)) {
        char *smsg = zstr_recv(sender);
        if (*smsg == '2') {
          // Source do teleport
          IMachine *tp_machine;

          // Check to see if VM is already up
          int i, md_index = -1;
          bool up = false;
          printf("Before searching, len %d\n", metadatas_len);
          for (i = 0; i < metadatas_len; i++) {
            if (streq(metadatas[i].name, vm_name)) {
              printf("Found md, home %s\n", metadatas[i].home);
              tp_session = metadatas[i].session;
              up = true;
              md_index = i;
            }
          }

          // Start VM if not up
          if (!up) {
            rc = IVirtualBoxClient_get_Session(vboxclient, &tp_session);
            if (FAILED(rc) || !tp_session) {
              err_exit("Could not get session for teleport.\n");
            }

            _find_machine(vm_name, &tp_machine);
            sleep(1);

            IProgress *tp_progress;
            BSTR type, env;
            g_pVBoxFuncs->pfnUtf8ToUtf16("gui", &type);
            rc = IMachine_LaunchVMProcess(tp_machine, tp_session, type, env, &tp_progress);
            if (FAILED(rc)) {
              log_err("Failed to start vm.");
            }
            g_pVBoxFuncs->pfnUtf16Free(type);

            IProgress_WaitForCompletion(tp_progress, -1);

            // Create metadata and add it to list
            struct VMMetadata vm_md = {name: vm_name, home: host_ip,
              n_history: 1, session: tp_session, guest: false
            };

            vm_md.history = malloc(1 * sizeof(char*));
            vm_md.history[0] = host_ip;
            add_metadata(vm_md);
          }

          // Commence teleporting
          IConsole *tp_console;
          rc = ISession_get_Console(tp_session, &tp_console);
          if (FAILED(rc) || !tp_console) {
            log_err("Couldnt get console for session in teleport.\n");
          }

          // To allow target's vm start
          if (up) {
            sleep(3);
          }

          IProgress *progress;
          BSTR hostnameu, passu;
          g_pVBoxFuncs->pfnUtf8ToUtf16(target_ip, &hostnameu);
          g_pVBoxFuncs->pfnUtf8ToUtf16(TP_PASS, &passu);
          rc = IConsole_Teleport(tp_console, hostnameu, TP_PORT, passu, 1000, &progress);
          if (FAILED(rc)) {
            log_err("Teleport Failed. ");
            printf("Return code %x\n", rc);
          }
          g_pVBoxFuncs->pfnUtf16Free(hostnameu);
          g_pVBoxFuncs->pfnUtf16Free(passu);

          // Wait for teleport to finish and check result
          IProgress_WaitForCompletion(progress, -1);
          int rescode = -1;
          IProgress_get_ResultCode(progress, &rescode);
          if (FAILED(rescode)) {
            printf("Return code %x\n", rc);
            IVirtualBoxErrorInfo *err;
            IProgress_get_ErrorInfo(progress, &err);
            if (err != NULL) {
              BSTR textu;
              char *text;
              IVirtualBoxErrorInfo_get_Text(err, &textu);
              g_pVBoxFuncs->pfnUtf16ToUtf8(textu, &text);
              printf("Error: %s\n", text);
              free(text);
            }
            err_exit("Teleport Failed\n");
          }

          // Send VM metadata
          TeleportMetadata md = TELEPORT_METADATA__INIT;
          void *buf;
          unsigned len;
          if (md_index != -1) {
            // Has previous metadata
            md.home = metadatas[md_index].home;
            md.n_history = metadatas[md_index].n_history;
            md.history = metadatas[md_index].history;
            metadatas[md_index].state = vmstate_teleported;

          } else {
            md.home = host_ip;
            md.n_history = 1;
            md.history = malloc(md.n_history * sizeof(char*));
            md.history[0] = host_ip;
          }
          len = teleport_metadata__get_packed_size(&md);
          buf = malloc(len);
          teleport_metadata__pack(&md, buf);

          /*printf("Sending md len %d\n", len);*/
          /*printf("%s\n----------\n", buf);*/
          zframe_t *frame = zframe_new(buf, len);
          zframe_send(&frame, sender, 0);
          /*zstr_send(sender, buf);*/
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

        OEMsg *oem;
        int len = strlen(smsg);
        if (!len) {
          log_err("Message rcvd from executer empty.\n");
        }

        oem = oemsg__unpack(NULL, len, smsg);
        if (oem->type == OEMSG__TYPE__TELEPORT) {
          bool ok = true;
          target_ip = oem->teleport->target_ip;
          if (streq(target_ip, "home")) {
            ok = false;
            int i;
            for (i = 0; i < metadatas_len; i++) {
              if (streq(metadatas[i].name, vm_name)) {
                target_ip = metadatas[i].home;
                ok = true;
              }
            }
          }
          if (ok) {
            vm_name = oem->teleport->vm_name;
            char addr[50] = {0};
            sprintf(addr, "tcp://%s:%d", target_ip, 23432);
            printf("Teleport to: %s\n", addr);
            sender = zsock_new_req(addr);
            zstr_send(sender, "1");
            zsock_wait(sender);
            zstr_send(sender, vm_name);
            zpoller_add(poller, sender);
          } else {
            log_err("Problem in finding the ip.\n");
          }
        }

        free(smsg);
        zmsg_destroy(&msg);
      }
    }
  }

  zpoller_destroy(&poller);
  zsock_destroy(&listener);
  printf("%s\n", "Actor finished.");
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
    int rescode = -1;
    IProgress_get_ResultCode(progress, &rescode);
    if (FAILED(rescode)) {
      printf("Return code %x\n", rc);
      IVirtualBoxErrorInfo *err;
      IProgress_get_ErrorInfo(progress, &err);
      if (err != NULL) {
        BSTR textu;
        char *text;
        IVirtualBoxErrorInfo_get_Text(err, &textu);
        g_pVBoxFuncs->pfnUtf16ToUtf8(textu, &text);
        printf("Error: %s\n", text);
        free(text);
      }
      err_exit("Teleport Failed\n");
    }
    /*ISession_UnlockMachine(tp_session);*/
}

void add_metadata(struct VMMetadata vm_md) {
  if (metadatas_len % 5 == 0) {
    metadatas = realloc(metadatas, (metadatas_len + 5) * sizeof(struct VMMetadata));
  }
  metadatas[metadatas_len++] = vm_md;
}

void start_vm(char *name) {
  // Find machine
  IMachine *machine;
  _find_machine(name, &machine);
  _start_vm(machine, session);
}

char *get_host_ip(char *host) {
  struct ifaddrs *ifaddr, *ifa;
  int s;
  /*int family;*/

  if (getifaddrs(&ifaddr) == -1)
  {
    perror("getifaddrs");
    exit(EXIT_FAILURE);
  }

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
  {
    if (ifa->ifa_addr == NULL)
      continue;

    s=getnameinfo(ifa->ifa_addr,sizeof(struct sockaddr_in),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

    if((strcmp(ifa->ifa_name,"eth0")==0 || strcmp(ifa->ifa_name,"p4p1")==0)&&(ifa->ifa_addr->sa_family==AF_INET))
    {
      if (s != 0)
      {
          printf("getnameinfo() failed: %s\n", gai_strerror(s));
          exit(EXIT_FAILURE);
      }
      freeifaddrs(ifaddr);
      return host;
    }
  }

  freeifaddrs(ifaddr);
  return NULL;
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
