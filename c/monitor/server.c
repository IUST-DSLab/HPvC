#include "server.h"
#include "monitor.pb-c.h"
#include <czmq.h>
#include <signal.h>
#include <string.h>

int set_host_utility(zloop_t* loop, zmq_pollitem_t* item, void* socket);
zhash_t* map;
int main(int argc, char *argv[]) {
  zctx_t* context = zctx_new();
  void* socket = zsocket_new(context, ZMQ_PULL);
  zsocket_bind(socket, "tcp://*:5050");
  signal(SIGINT, exit);
  printf("Starting server...\n");

  map = zhash_new();

  zloop_t* loop = zloop_new();
  
  zloop_set_verbose(loop, 1);
  zloop_timer(loop, 10000, 1, set_host_utility, NULL);

  zmq_pollitem_t poll = {socket, 0, ZMQ_POLLIN};
  zloop_poller(loop, &poll, set_host_utility, socket);

  zloop_start(loop);
  zloop_destroy(&loop);

  zsocket_destroy(context, socket);
  zctx_destroy(&context);

  return 0;
}


int set_host_utility(zloop_t* loop, zmq_pollitem_t* item, void* socket) {
  HostUtility *host_utility = NULL;
  int len;
  unsigned char* msg = (unsigned char*)zstr_recv(socket);
  len = strlen((char*)msg);
  host_utility = host_utility__unpack(NULL, len, msg);
  if(host_utility == NULL) {
    printf("Invalid host utility!\n");
    fwrite(msg,len,1,stdout);
  }
  // zhash_insert(map, host_utility->ip, host_utility->ip);
  // printf("%s\n", zhash_lookup(map, "127.0.0.1"));
  return 0;
}