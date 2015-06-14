/**
 * Copyright (c) 2015-2016. The DSLab HPC Team
 * All rights reserved. 
 * Developers: Aryan Baghi
 * This is file we used to develop the monitor deamon 
 * */

#include "monitor.h"

#include <czmq.h>
#include "VBoxCAPIGlue.h"
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include "monitor.pb-c.h"


/* This struct will use as link list
to grap metrics data for each machine */
typedef struct Machine
{
  MachineMetric metrics;
  struct Machine *next;
} Machine;

IVirtualBoxClient *vboxclient = NULL;
IVirtualBox *vbox = NULL;
IPerformanceCollector *pc = NULL;
SAFEARRAY *metric_names = NULL,
  *objects = NULL, *units = NULL,
  *scales = NULL, *sequence_numbers = NULL,
  *indices = NULL, *length = NULL, *data = NULL;

zctx_t* context  = NULL;

void _setup_performance_metrics();
void _setup_query_metrics();
void _send_host_metrics();
void _get_metrics(HostMetric *metrics);
Machine *find_machine(Machine *machines, char *uuid);
Machine *last_machine(Machine *machines);
void err_exit(char *err);
void log_err(char *err);


int main(int argc, char *argv[]) {

  ULONG revision = 0;
  HRESULT rc;

  context = zctx_new();

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
    err_exit("Could not get VirtualBox reference.");
  }

  rc = IVirtualBox_get_PerformanceCollector(vbox, &pc);
  if (FAILED(rc)) {
    err_exit("Could not get PerformanceCollector refrence.");
  }

  // Initialize out array needed for virtual box metrics
  metric_names = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
  objects = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
  units = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
  scales = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
  sequence_numbers = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
  indices = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
  length = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
  data = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
    
  // Setup metric we care about
  _setup_performance_metrics();  

  while(TRUE) {
    _setup_query_metrics();
    
    // sleep for SLEEP_TIME and send metrics again
    sleep(SLEEP_TIME*6);

    // send metrics data to server
    _send_host_metrics();
  }

  // Destory out array we initialized in lines 70-78
  g_pVBoxFuncs->pfnSafeArrayDestroy(metric_names);
  g_pVBoxFuncs->pfnSafeArrayDestroy(objects);
  g_pVBoxFuncs->pfnSafeArrayDestroy(units);
  g_pVBoxFuncs->pfnSafeArrayDestroy(scales);
  g_pVBoxFuncs->pfnSafeArrayDestroy(sequence_numbers);
  g_pVBoxFuncs->pfnSafeArrayDestroy(indices);
  g_pVBoxFuncs->pfnSafeArrayDestroy(length);
  g_pVBoxFuncs->pfnSafeArrayDestroy(data);

  zctx_destroy(&context);

  return 0;
}


/*
This function will grab all metric we need
and send it to server
*/
void _send_host_metrics() {

  // Initialize HostMetric message contain all info about this host
  HostMetric metric = HOST_METRIC__INIT;
  // Variable needed for protobuf
  unsigned char *buf;
  unsigned int len;

  HRESULT rc;

  void *request = zsocket_new(context, ZMQ_PUSH);
  zmsg_t *msg = zmsg_new ();

  signal(SIGINT, exit);
  // Connect to server
  zsocket_connect(request, "tcp://localhost:5050");

  // Get HostMetrics data
  _get_metrics(&metric); 
  // Pack HostMetric data into buf
  len = host_metric__get_packed_size(&metric);
  buf = malloc(len);
  host_metric__pack(&metric, buf);
  
  if(zmsg_addmem(msg, buf, len) != 0){
    printf("error\n");
  }

  // send message to server
  zmsg_send (&msg, request);
  // free memory
  free(buf);
  // TODO: destory and free metric (HostMetric)
  zmsg_destroy(&msg);
  zsocket_destroy(context, request);
}

void _setup_performance_metrics() {

  HRESULT rc;

  SAFEARRAY *metric_names = g_pVBoxFuncs->pfnSafeArrayCreateVector(VT_BSTR, 0, 0);
  SAFEARRAY *objects = g_pVBoxFuncs->pfnSafeArrayCreateVector(VT_UNKNOWN, 0, 0);
  SAFEARRAY *metrics = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();

  rc = IPerformanceCollector_SetupMetrics(
    pc,
    ComSafeArrayAsInParam(metric_names),
    ComSafeArrayAsInParam(objects),
    PERIOD,
    COUNT,
    ComSafeArrayAsOutIfaceParam(metrics, IPerformanceMetric *)
  );
  if (FAILED(rc)) {
    err_exit("Could not setup performance metrics.");
  }

  g_pVBoxFuncs->pfnSafeArrayDestroy(metrics);
  g_pVBoxFuncs->pfnSafeArrayDestroy(metric_names);
  g_pVBoxFuncs->pfnSafeArrayDestroy(objects);

}

void _setup_query_metrics() {

  HRESULT rc;

  /* set in metrics and in objects to empty array because we want all metrics for all objects
  (Host and Guests)*/
  SAFEARRAY *in_metric_names = g_pVBoxFuncs->pfnSafeArrayCreateVector(VT_BSTR, 0, 0); // in
  SAFEARRAY *in_objects = g_pVBoxFuncs->pfnSafeArrayCreateVector(VT_UNKNOWN, 0, 0); // in


  rc = IPerformanceCollector_QueryMetricsData(
    pc,
    ComSafeArrayAsInParam(in_metric_names),
    ComSafeArrayAsInParam(in_objects), 
    ComSafeArrayAsOutIfaceParam(metric_names, BSTR),
    ComSafeArrayAsOutIfaceParam(objects, nsISupports*),
    ComSafeArrayAsOutIfaceParam(units, BSTR),
    ComSafeArrayAsOutIfaceParam(scales, unsigned int),
    ComSafeArrayAsOutIfaceParam(sequence_numbers, unsigned int),
    ComSafeArrayAsOutIfaceParam(indices, unsigned int),
    ComSafeArrayAsOutIfaceParam(length, unsigned int),
    ComSafeArrayAsOutIfaceParam(data, int)
  );
  if (FAILED(rc)) {
    err_exit("Could not get query metrics data.");
  }

  g_pVBoxFuncs->pfnSafeArrayDestroy(in_metric_names);
  g_pVBoxFuncs->pfnSafeArrayDestroy(in_objects);

}

void _get_metrics(HostMetric *metrics) {
  double res = 0;
  int *_data, i, j;
  BSTR *names, ip, uuid;
  char *name, *cip, *cuuid;
  ULONG data_length, metric_names_length, *_length, *_indices, *_scales, network_interfaces_length, machines_length = 0;
  IUnknown **_objects;
  void *machine = NULL;
  IHostNetworkInterface **_network_interfaces = NULL;
  SAFEARRAY *network_interfaces = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
  Machine *machines = NULL, *found_machine = NULL;

  g_pVBoxFuncs->pfnSafeArrayCopyOutIfaceParamHelper((IUnknown ***)&_data, &data_length, data);
  g_pVBoxFuncs->pfnSafeArrayCopyOutIfaceParamHelper((IUnknown ***)&names, &metric_names_length, metric_names);
  g_pVBoxFuncs->pfnSafeArrayCopyOutIfaceParamHelper((IUnknown ***)&_scales, &metric_names_length, scales);
  g_pVBoxFuncs->pfnSafeArrayCopyOutIfaceParamHelper((IUnknown ***)&_indices, &metric_names_length, indices);
  g_pVBoxFuncs->pfnSafeArrayCopyOutIfaceParamHelper((IUnknown ***)&_length, &metric_names_length, length);
  g_pVBoxFuncs->pfnSafeArrayCopyOutIfaceParamHelper((IUnknown ***)&_objects, &metric_names_length, objects);
  for (i=0, res=0; i<metric_names_length; i++) {
    g_pVBoxFuncs->pfnUtf16ToUtf8(names[i], &name);
    
    printf("===========================\n");
    printf("%d: %s\n", i, name);
    printf("data[i] = %d\n", _data[i]);
    printf("indices: %d - length: %d\n", _indices[i], _length[i]);
    printf("%d\n", _scales[i]);
    for (j=_indices[i]; j<_indices[i]+_length[i]; j++) {
      res += _data[j];
    }
    if (_length[i] > 0) {
      res = res / _length[i];
    }

    // If Guest 
    IUnknown_QueryInterface(_objects[i], &IID_IMachine, &machine);
    if (machine != NULL) {
      if (machines == NULL) {
        machines_length++;
        machines = (Machine *) malloc(sizeof(Machine)); 
        machines->next = NULL;
        machine_metric__init(&machines->metrics);
        IMachine_get_Id((IMachine *) machine, &uuid);
        g_pVBoxFuncs->pfnUtf16ToUtf8(uuid, &cuuid);
        machines->metrics.uuid = malloc(strlen(cuuid));
        strcpy(machines->metrics.uuid, cuuid);
        printf("%s\n", machines->metrics.uuid);
        found_machine = machines;
      }
      else {
        IMachine_get_Id((IMachine *) machine, &uuid);
        g_pVBoxFuncs->pfnUtf16ToUtf8(uuid, &cuuid);
        found_machine = find_machine(machines, cuuid);
        if (found_machine == NULL) {
          machines_length++;
          found_machine = last_machine(machines);
          found_machine->next = (Machine *) malloc(sizeof(Machine));
          machine_metric__init(&machines->metrics);
          found_machine = found_machine->next;
        }
      }
      if (strcmp(name, "Guest/RAM/Usage/Total:avg") == 0) {
        found_machine->metrics.ram_usage_total_average = res;
      }
      else if (strcmp(name, "Guest/CPU/Load/User:avg") == 0) {
        found_machine->metrics.cpu_load_usage = res;
      }
      found_machine->metrics.ip = "127.0.0.1";
      found_machine->metrics.cpu = 2;
      found_machine->metrics.cpu_load_usage = 12;
      found_machine->metrics.ram = 12;
      found_machine->metrics.ram_usage_total_average = 12;
    }
    // If Host
    else {
      // double check if host
      IUnknown_QueryInterface(_objects[i], &IID_IHost, &machine);
      if (machine != NULL) {
        IHost_get_NetworkInterfaces((IHost *) machine, ComSafeArrayAsOutIfaceParam(network_interfaces, IHostNetworkInterface *));
        g_pVBoxFuncs->pfnSafeArrayCopyOutIfaceParamHelper((IUnknown ***)&_network_interfaces, &network_interfaces_length, network_interfaces);
        if (network_interfaces_length > 0) {
          IHostNetworkInterface_get_IPAddress(_network_interfaces[0], &ip);
          g_pVBoxFuncs->pfnUtf16ToUtf8(ip, &cip);
          metrics->ip = cip;
          printf("ip: %s\n", metrics->ip);
        }
        // g_pVBoxFuncs->pfnUtf16ToUtf8(a, &name);
        // printf("host os name: %s\n", name);
        if (strcmp(name, "RAM/Usage/Total:avg") == 0) {
          metrics->ram_usage_total_average = res;
        }
        else if (strcmp(name, "CPU/Load/User:avg") == 0) {
          metrics->cpu_load_usage = res;
        }
        IHost_get_MemorySize((IHost *) machine, &metrics->ram);
        metrics->core_per_cpu = 1;
        metrics->cpu = 1;
      }
    }
  }

  metrics->n_machines = machines_length;
  metrics->machines = (MachineMetric **) malloc(sizeof(MachineMetric *) * machines_length);

  for (i=0; i<metrics->n_machines; i++) {
    metrics->machines[i] = &machines->metrics;
    machines = machines->next;
  }
  int len = host_metric__get_packed_size(metrics);
  char* buf = malloc(len);
  host_metric__pack(metrics, buf);
  // for (i=0; i<metric_names_length; i++) {
  //   free(names[i]);
  // }


  // free(names);
  // free(name);
  free(_data);
  free(_length);
  free(_indices);
  free(_scales);
}

Machine *find_machine(Machine *machines, char *uuid) {
  if (machines == NULL) {
    return NULL;
  }

  while(machines != NULL) {
    if (strcmp(machines->metrics.uuid, uuid) == 0) {
      return machines;
    }
    machines = machines->next;
  }
  return NULL;
}

Machine *last_machine(Machine *machines) {
  while (machines->next != NULL) {
    machines = machines->next;
  }
  return machines;
}

void err_exit(char *err) {
  log_err(err);
  _exit(1);
}

void log_err(char *err) {
  fprintf(stderr, err);
}
