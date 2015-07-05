#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "zmq.h"
int main (int argc, char const *argv[]) {
  void* context = zmq_ctx_new();

  void* socket = zmq_socket(context, ZMQ_PUSH);
  zmq_connect(socket, "tcp://localhost:5051");

  zmq_msg_t message;
  char* ssend = "T";
  int t_length = strlen(ssend);
  zmq_msg_init_size(&message, t_length);
  memcpy(zmq_msg_data(&message), ssend, t_length);
  zmq_msg_send(&message, socket, 0);
  zmq_msg_close(&message);
  zmq_close(socket);
  zmq_ctx_destroy(context);
  return 0; 
}
