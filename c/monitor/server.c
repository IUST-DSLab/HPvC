#include "server.h"
#include "monitor.pb-c.h"
#include <zmq.h>
#include <signal.h>
#include <string.h>

typedef struct Host
{
  HostMetric *metric;
  struct Host *next;
} Host;

int response_to_api(zmq_msg_t *msg, void* api_socket);
int set_host_utility(zmq_msg_t *msg);
void err_exit(char *err);
void log_err(char *err);
Host *find_host(Host *hosts, char *ip);
MachineMetric *find_machine(Host *hosts, char *ip);
void hm_free(HostMetric **metric);

Host* hosts;

int main(int argc, char *argv[]) {

  pid_t pid;
  // Context is thread safe!
  void* context = zmq_ctx_new();
  void* socket = zmq_socket(context, ZMQ_PULL);
  zmq_bind(socket, "tcp://*:5050");
  // API socket creation and binding
  void* api_socket = zmq_socket(context, ZMQ_REP);
  zmq_bind(api_socket, "tcp://*:5051");

  //signal(SIGINT, exit);
  printf("Starting server...\n");

  zmq_pollitem_t polls[2];
  polls[0].socket = socket;
  polls[0].fd = 0;
  polls[0].events = ZMQ_POLLIN;
  polls[0].revents = 0;

  polls[1].socket = api_socket;
  polls[1].fd = 0;
  polls[1].events = ZMQ_POLLIN;
  polls[1].revents = 0;
  
  for(;;) {
    zmq_msg_t msg;
    int res =  zmq_poll(polls, 2, -1);
    printf("first run\n");
    if(polls[0].revents) {
      zmq_msg_init(&msg);
      zmq_msg_recv(&msg, socket, 0);
      set_host_utility(&msg);
      zmq_msg_close(&msg);
    }
    if(polls[1].revents > 0) {
      zmq_msg_init(&msg);
      zmq_msg_recv(&msg, api_socket, 0);
      response_to_api(&msg, api_socket);
      zmq_msg_close(&msg);
    }
  }
  zmq_close(api_socket);
  zmq_close(socket);
  zmq_ctx_destroy(context);
  
  return 0;
}

int response_to_api(zmq_msg_t *msg, void* api_socket) {
  zmq_msg_t reply;
  InterfaceMessage *message = NULL;
  char *buf;
  unsigned int len = zmq_msg_size(msg);
  unsigned char *packed_msg = (unsigned char *) malloc(len+1);
  memcpy(packed_msg, zmq_msg_data(msg), len);
  message = interface_message__unpack(NULL, len, packed_msg);
  printf("ip: %s", message->ip);

  if (message->is_host == 1) {
    Host *response_host = find_host(hosts, message->ip);
    if (response_host != NULL) {
      printf("host find\n");
  //     int t_length = host_metric__get_packed_size(&response_host);
  //     buf = malloc(t_length);
  //     host_metric__pack(&response_host, buf);
  //     zmq_msg_init_size(&reply, t_length);
  //     if(zmsg_addmem(&reply, buf, len) != 0){
  //       printf("error\n");
  //     }
    } else {
      printf("host not found\n");
  //     HostMetric response;
  //     interface_message__init(&response);
    }
  } else {
    MachineMetric *response_machine = find_machine(hosts, message->ip);
    if (response_machine != NULL) {
      printf("machin found\n");
      int t_length = machine_metric__get_packed_size(response_machine);
      buf = malloc(t_length);
      machine_metric__pack(response_machine, buf);
      zmq_msg_init_size(&reply, t_length);
      memcpy(zmq_msg_data(&reply), buf, t_length);
    } else {
      printf("machine not found\n");
    }
  }

  // zmq_msg_init_size(&reply, strlen("world"));
  // memcpy(zmq_msg_data(&reply), "world", 5);
  zmq_msg_send(&reply, api_socket, 0);
  zmq_msg_close(&reply);

  return 0;
}

int set_host_utility(zmq_msg_t *msg) {

  Host *host = NULL, *host_tmp = NULL;
  int i = 0, j = 0;
  unsigned int len = zmq_msg_size(msg);
  unsigned char *packed_msg = (unsigned char *) malloc(len+1);
  memcpy(packed_msg, zmq_msg_data(msg), len);

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
      // Update physical node info
      // Update IP if not equal
      if(strcmp(host_tmp->metric->ip, host->metric->ip) != 0) {
        free(host_tmp->metric->ip);
        host_tmp->metric->ip = (char*) malloc(sizeof(char) * strlen(host->metric->ip));
        strcpy(host_tmp->metric->ip, host->metric->ip);
      }
      host_tmp->metric->cpu = host->metric->cpu;
      host_tmp->metric->ram = host->metric->ram;
      host_tmp->metric->core_per_cpu = host->metric->core_per_cpu;
      host_tmp->metric->cpu_load_usage =
        ((AVERAGE_RATE-1)*host->metric->cpu_load_usage + host_tmp->metric->cpu_load_usage) / AVERAGE_RATE;
      host_tmp->metric->ram_usage_total_average =
        ((AVERAGE_RATE-1)*host->metric->ram_usage_total_average + host_tmp->metric->ram_usage_total_average) / AVERAGE_RATE;

      for(i=0; i<host->metric->n_machines; i++) {
        // Find machine
        int machine_index = -1;
        for(j=0; j<host_tmp->metric->n_machines; j++) {
          if(strcmp(host->metric->machines[i]->uuid, host_tmp->metric->machines[j]->uuid) == 0) {
            machine_index = j;
            break;
          }
        }
        // If machine not found
        if(machine_index == -1) {
          // Extend machines array
          host_tmp->metric->n_machines += 1;
          MachineMetric **temp = (MachineMetric **) malloc(sizeof(MachineMetric*) * host_tmp->metric->n_machines);
          memcpy(temp, host_tmp->metric->machines, sizeof(MachineMetric*) * (host_tmp->metric->n_machines-1));
          // Append new machine to machines list
          host_tmp->metric->machines[host_tmp->metric->n_machines-1] = (MachineMetric *) malloc(sizeof(MachineMetric));
          host_tmp->metric->machines[host_tmp->metric->n_machines-1]->cpu = host->metric->cpu;
          host_tmp->metric->machines[host_tmp->metric->n_machines-1]->ram = host->metric->ram;
          host_tmp->metric->machines[host_tmp->metric->n_machines-1]->cpu_load_usage = host_tmp->metric->cpu_load_usage;
          host_tmp->metric->machines[host_tmp->metric->n_machines-1]->ram_usage_total_average = host_tmp->metric->ram_usage_total_average;

        }
        // Else machine found
        else {
          host_tmp->metric->machines[machine_index]->cpu_load_usage =
            ((AVERAGE_RATE-1)*host->metric->machines[machine_index]->cpu_load_usage +
              host_tmp->metric->machines[machine_index]->cpu_load_usage) / AVERAGE_RATE;
          host_tmp->metric->machines[machine_index]->ram_usage_total_average =
            ((AVERAGE_RATE-1)*host->metric->machines[machine_index]->ram_usage_total_average +
              host_tmp->metric->machines[machine_index]->ram_usage_total_average) / AVERAGE_RATE;
          host_tmp->metric->machines[machine_index]->cpu = host->metric->machines[machine_index]->cpu;
          host_tmp->metric->machines[machine_index]->ram = host->metric->machines[machine_index]->ram;
          // Update Ip
          free(host_tmp->metric->machines[machine_index]->ip);
          host_tmp->metric->machines[machine_index]->ip = (char*) malloc(sizeof(char) * strlen(host->metric->machines[machine_index]->ip));
          strcpy(host_tmp->metric->machines[machine_index]->ip, host->metric->machines[machine_index]->ip);
        
        }
      }
      // hm_free(&host_tmp->metric);
      // host_tmp->metric = host->metric;
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
  while (hosts != NULL) {
    if (strcmp(hosts->metric->ip, ip) == 0) {
      return hosts;
    }
    hosts = hosts->next;
  }
  return NULL;
}

MachineMetric *find_machine(Host *hosts, char *ip) {
  int i = 0;
  while (hosts != NULL) {
    printf("host check\n");
    for (i=0; i<hosts->metric->n_machines;i++) {
      printf("%s in\n", hosts->metric->machines[i]->ip);
      if (strcmp(hosts->metric->machines[i]->ip, ip) == 0) {
        return hosts->metric->machines[i];
      }
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
