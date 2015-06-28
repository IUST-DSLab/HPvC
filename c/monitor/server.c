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

      for(int i=0; i<host->metric->n_machines; i++) {
        // Find machine
        int machine_index = -1;
        for(int j=0; j<host_tmp->metric->n_machines; j++) {
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