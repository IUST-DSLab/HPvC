#include "VBoxCAPIGlue.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>



void _exit(int status);
void log_err(char *err);
void err_exit(char *err);

IMachine *find_machine(char *vmname);

IVirtualBoxClient *vboxclient = NULL;
IVirtualBox *vbox = NULL;
ISession *session = NULL;

int main(int argc, char *argv[]) {
    ULONG revision = 0;
    HRESULT rc;

    // Initialize objects
    if (VBoxCGlueInit()) {
      err_exit(g_szVBoxErrMsg);
    }

    unsigned ver = g_pVBoxFuncs->pfnGetVersion();
    printf("VirtualBox version: %u.%u.%u\n", ver / 1000000, ver / 1000 % 1000, ver % 1000);
    ver = g_pVBoxFuncs->pfnGetAPIVersion();
    printf("VirtualBox API version: %u.%u\n", ver / 1000, ver % 1000);

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
    rc = IVirtualBox_get_Revision(vbox, &revision);
    if (SUCCEEDED(rc)) {
      printf("Revision: %u\n", revision);
    }

    // Find machine
    IMachine *machine;
    BSTR snapshot_folder_utf16;
    char *snapshot_folder;
    machine = find_machine("Fedora");
    rc = IMachine_get_SnapshotFolder(machine, &snapshot_folder_utf16);
    if (FAILED(rc) | !snapshot_folder_utf16) {
      err_exit("Failed to get memory size of machine.");
    }
    g_pVBoxFuncs->pfnUtf16ToUtf8(snapshot_folder_utf16, &snapshot_folder);
    printf("Snapshot Folder of Fedora: %s\n", snapshot_folder);
    g_pVBoxFuncs->pfnUtf8Free(snapshot_folder);
    g_pVBoxFuncs->pfnComUnallocString(snapshot_folder_utf16);


    _exit(0);
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

  g_pVBoxFuncs->pfnClientUninitialize();
  VBoxCGlueTerm();
  exit(status);
}

void log_err(char *err) {
  fprintf(stderr, err);
}

void err_exit(char *err) {
  log_err(err);
  _exit(1);
}

IMachine *find_machine(char *vmname) {
  BSTR vmname_utf16;
  g_pVBoxFuncs->pfnUtf8ToUtf16(vmname, &vmname_utf16);
  rc = IVirtualBox_FindMachine(vbox, vmname_utf16, &machine);
  if (FAILED(rc) | !machine) {
    err_exit("Could not find machine.");
  }
  g_pVBoxFuncs->pfnUtf16Free(vmname_utf16);
}
