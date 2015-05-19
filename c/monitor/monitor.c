#include "monitor.h"
#include <czmq.h>
#include "VBoxCAPIGlue.h"
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include "monitor.pb-c.h"

IVirtualBoxClient *vboxclient = NULL;
IVirtualBox *vbox = NULL;
ISession *session = NULL;
ISession *tp_session = NULL;
IHost *host = NULL;

void err_exit(char *err);
void log_err(char *err);


int main(int argc, char *argv[]) {

  ULONG revision = 0;
  HRESULT rc;

  unsigned char *buf;
  int len;
  unsigned int cpu;

  HostUtility host_utility = HOST_UTILITY__INIT;

  zctx_t* context = zctx_new();
  void* request = zsocket_new(context, ZMQ_PUSH);
  signal(SIGINT, exit);
  printf("Starting client...\n");
  zsocket_connect(request, "tcp://localhost:5050");

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

  while(1) {
    IVirtualBox_get_Host(vbox, &host);
    IHost_get_ProcessorCount(host, &cpu);
    printf("Processor Count: %d\n", cpu);
    
    // test data
    host_utility.ip = "127.0.0.1";
    host_utility.cpu = cpu;
    host_utility.core_per_cpu = 1;
    host_utility.cpu_usage = 42.0;
    host_utility.ram = 12;
    host_utility.ram_usage = 12;
    len = host_utility__get_packed_size(&host_utility);

    buf = malloc(len);
    host_utility__pack(&host_utility, buf);
    fwrite(buf,len,1,stdout);
    zstr_send(request, (char *)buf);
    printf("Pushing Count\n");
    sleep(REFRESH);
    free(buf);
  }
  zsocket_destroy(context, request);
  zctx_destroy(&context);

  return 0;
}

void err_exit(char *err) {
  log_err(err);
  _exit(1);
}

void log_err(char *err) {
  fprintf(stderr, err);
}
