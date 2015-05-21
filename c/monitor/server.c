#include "server.h"
#include "monitor.pb-c.h"
#include <czmq.h>
#include <signal.h>
#include <string.h>

int set_host_utility(zloop_t* loop, zmq_pollitem_t* item, void* socket);
void err_exit(char *err);
void log_err(char *err);

zhash_t* host_utility;

int main(int argc, char *argv[]) {

  zctx_t* context = zctx_new();
  void* socket = zsocket_new(context, ZMQ_PULL);
  zsocket_bind(socket, "tcp://*:5050");
  signal(SIGINT, exit);
  printf("Starting server...\n");


  host_utility = zhash_new();

  zloop_t* loop = zloop_new();
  
  // zloop_set_verbose(loop, 1);
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
  HostMetric *host_metric_msg = NULL;
  zmsg_t* msg = zmsg_recv(socket);
  unsigned int len = zmsg_content_size(msg);
  unsigned char* packed_msg = zmsg_popstr(msg);

  printf("%d %s\n",len, packed_msg);

  host_metric_msg = host_metric__unpack(NULL, len, packed_msg);

  if(host_utility == NULL) {
    log_err("Invalid host utility message!\n");
  } else {
    zhash_insert(host_utility, host_metric_msg->ip, host_metric_msg);
    printf("Host %s registerd!\n", host_metric_msg->ip);
  }

  return 0;
}

void err_exit(char *err) {
  log_err(err);
  exit(1);
}

void log_err(char *err) {
  fprintf(stderr, err);
}