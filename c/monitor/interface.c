#include "interface.h"
#include "monitor.pb-c.h"
#include <zmq.h>
#include <string.h>

// char* getDataFromServer() {
//   void* context = zmq_ctx_new();

//   void* socket = zmq_socket(context, ZMQ_REQ);
//   zmq_connect(socket, "tcp://localhost:5051");

//   zmq_msg_t message;
//   char* ssend = "T";
//   int t_length = strlen(ssend);
//   zmq_msg_init_size(&message, t_length);
//   memcpy(zmq_msg_data(&message), ssend, t_length);
//   zmq_msg_send(&message, socket, 0);
//   zmq_msg_close(&message);

//   zmq_msg_t reply;
//   zmq_msg_init(&reply);
//   zmq_msg_recv(&reply, socket, 0);
//   unsigned int len = zmq_msg_size(&reply);
//   unsigned char *packed_msg = (unsigned char *) malloc(len+1);
//   memcpy(packed_msg, zmq_msg_data(&reply), len);
//   printf("%s", packed_msg);
//   zmq_msg_close(&reply);

//   zmq_close(socket);
//   zmq_ctx_destroy(context);
//   return packed_msg; 
// }

void* getMetricPackedMessageFromServer(int is_host, char *ip) {
  void* context = zmq_ctx_new();
  MachineMetric *machine = NULL;
  HostMetric *host = NULL;
  void* socket = zmq_socket(context, ZMQ_REQ);
  zmq_connect(socket, "tcp://localhost:5051");
  char *buf;
  InterfaceMessage im;
  interface_message__init(&im);
  im.is_host = is_host;
  im.ip = ip;
  zmq_msg_t message;
  int t_length = interface_message__get_packed_size(&im);
  buf = malloc(t_length);
  interface_message__pack(&im, buf);
  zmq_msg_init_size(&message, t_length);
  memcpy(zmq_msg_data(&message), buf, t_length);
  zmq_msg_send(&message, socket, 0);
  zmq_msg_close(&message);
  zmq_msg_t reply;
  zmq_msg_init(&reply);
  zmq_msg_recv(&reply, socket, 0);
  unsigned int len = zmq_msg_size(&reply);
  unsigned char *packed_msg = (unsigned char *) malloc(len+1);
  memcpy(packed_msg, zmq_msg_data(&reply), len);
  zmq_msg_close(&reply);
  free(buf);
  if (is_host == 0) {
    
    machine = machine_metric__unpack(NULL, len, packed_msg);
    return machine;
  } else {
    
    machine = host_metric__unpack(NULL, len, packed_msg);
    return host;
  }

  
}

// void getHostMetric(char *ip, HostMetric *result) {
  
// }

// void getMachinMetric(char *ip, HostMetric *result) {
//   zmq_msg_t message;
//   char* ssend = "T";
//   int t_length = strlen(ssend);
//   zmq_msg_init_size(&message, t_length);
//   memcpy(zmq_msg_data(&message), ssend, t_length);
//   zmq_msg_send(&message, socket, 0);
//   zmq_msg_close(&message);

//   zmq_msg_t reply;
//   zmq_msg_init(&reply);
//   zmq_msg_recv(&reply, socket, 0);
//   unsigned int len = zmq_msg_size(&reply);
//   unsigned char *packed_msg = (unsigned char *) malloc(len+1);
//   memcpy(packed_msg, zmq_msg_data(&reply), len);
//   printf("%s", packed_msg);
//   zmq_msg_close(&reply);
// }

int main() {
  MachineMetric *machine = (MachineMetric*) getMetricPackedMessageFromServer(0,"127.0.0.1");
  printf("%s\n", machine->uuid);
  return 0;
}
