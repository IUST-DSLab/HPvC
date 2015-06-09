#include "server.h"
#include "monitor.pb-c.h"
#include <czmq.h>
#include <signal.h>
#include <string.h>

typedef struct Host
{
  HostMetric *metric;
  struct Host *next;
} Host;

int set_host_utility(zloop_t* loop, zmq_pollitem_t* item, void* socket);
void err_exit(char *err);
void log_err(char *err);
Host *find_host(Host *hosts, char *ip);
void hm_free(HostMetric **metric);

Host* hosts;

int main(int argc, char *argv[]) {

  zctx_t* context = zctx_new();
  void* socket = zsocket_new(context, ZMQ_PULL);
  zsocket_bind(socket, "tcp://*:5050");
  signal(SIGINT, exit);
  printf("Starting server...\n");


  zloop_t* loop = zloop_new();
  
  zloop_set_verbose(loop, 1);
  // zloop_timer(loop, 10000, 1, set_host_utility, NULL);

  zmq_pollitem_t poll = {socket, 0, ZMQ_POLLIN};
  zloop_poller(loop, &poll, set_host_utility, socket);

  zloop_start(loop);
  zloop_destroy(&loop);

  zsocket_destroy(context, socket);
  zctx_destroy(&context);

  return 0;
}


int set_host_utility(zloop_t* loop, zmq_pollitem_t* item, void* socket) {
  Host *host = NULL, *host_tmp = NULL;
  zmsg_t *msg = zmsg_recv(socket);
  unsigned int len = zmsg_content_size(msg);
  unsigned char *packed_msg = (unsigned char *) zmsg_popstr(msg);

  host = (Host *) malloc(sizeof(Host));
  host->metric = host_metric__unpack(NULL, len, packed_msg);
  host->next = NULL;

  if(host->metric == NULL) {
    log_err("Invalid host utility message!\n");
    return 1;
  } else {
    host_tmp = find_host(hosts, host->metric->ip);
    // Host not exist
    if (host_tmp == NULL)
    {
      // If hosts is already empty
      if (hosts == NULL) {
        hosts = host;
      }
      // If hosts is not empty append host to hosts
      else {
        host_tmp = hosts;
        while (host_tmp->next != NULL) {
          host_tmp = host_tmp->next;
        }
        host_tmp->next = host;
      }
    }
    // Host exist
    else {
      hm_free(&host_tmp->metric);
      host_tmp->metric = host->metric;
    }
  }
  printf("Host %s updated!\n", host->metric->ip);
  // free(host);
  // free(host_tmp);
  // free(packed_msg);
  // zmsg_destroy(&msg);
  return 0;
}

Host *find_host(Host *hosts, char *ip) {
  if (hosts == NULL) {
    return NULL;
  }
  while (hosts->next != NULL) {
    if (strcmp(hosts->metric->ip, ip) == 0) {
      return hosts;
    }
    hosts = hosts->next;
  }
  return NULL;
}

void hm_free(HostMetric **metric) {
  // should free metric
}

void err_exit(char *err) {
  log_err(err);
  exit(1);
}

void log_err(char *err) {
  fprintf(stderr, err);
}