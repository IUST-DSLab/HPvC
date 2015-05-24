#include "monitor.h"
#include <czmq.h>
#include "VBoxCAPIGlue.h"
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include "monitor.pb-c.h"

IVirtualBoxClient *vboxclient = NULL;
IVirtualBox *vbox = NULL;
IPerformanceCollector *pc = NULL;
SAFEARRAY *metric_names = NULL;
SAFEARRAY *objects = NULL;
SAFEARRAY *units = NULL;
SAFEARRAY *scales = NULL;
SAFEARRAY *sequence_numbers = NULL;
SAFEARRAY *indices = NULL;
SAFEARRAY *length = NULL;
SAFEARRAY *data = NULL;

zctx_t* context  = NULL;

void err_exit(char *err);
void log_err(char *err);
void _setup_performance_metrics();
void _setup_query_metrics();
void _send_host_metrics();
HostMetric _get_metrics();


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

  metric_names = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
  objects = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
  units = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
  scales = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
  sequence_numbers = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
  indices = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
  length = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
  data = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
    
  _setup_performance_metrics();  

  while(TRUE) {
    _setup_query_metrics();
    
    sleep(SLEEP_TIME*6);

    printf("Sending metrics ...\n");
    _send_host_metrics();
    printf("Sent!\n");
  }

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


void _send_host_metrics() {

  HostMetric metric;
  unsigned char *buf;
  unsigned int len;
  HRESULT rc;

  void *request = zsocket_new(context, ZMQ_PUSH);
  zmsg_t *msg = zmsg_new ();

  signal(SIGINT, exit);
  zsocket_connect(request, "tcp://localhost:5050");

  metric = _get_metrics(); 

  len = host_metric__get_packed_size(&metric);
  buf = malloc(len);

  host_metric__pack(&metric, buf);
  
  if(zmsg_addmem(msg, buf, len) != 0){
    printf("error\n");
  }


  zmsg_send (&msg, request);

  free(buf);
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

HostMetric _get_metrics() {
  double res = 0;
  int *_data, i, j;
  BSTR *names, ip;
  char *name, *cip;
  ULONG data_length, metric_names_length, *_length, *_indices, *_scales, network_interfaces_length;
  IUnknown **_objects;
  void *machine = NULL;
  HostMetric metrics = HOST_METRIC__INIT; 
  IHostNetworkInterface **_network_interfaces = NULL;
  SAFEARRAY *network_interfaces = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();

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

    IUnknown_QueryInterface(_objects[i], &IID_IMachine, &machine);
    if (machine != NULL) {
      // IMachine_get_Name((IMachine *)machine, &a);
      // printf("machine name: %s\n", name);
      if (strcmp(name, "Guest/RAM/Usage/Total:avg") == 0) {

      }
      else if (strcmp(name, "Guest/CPU/Load/User:avg") == 0) {
        
      }
    }
    else {
      IUnknown_QueryInterface(_objects[i], &IID_IHost, &machine);
      if (machine != NULL) {
        IHost_get_NetworkInterfaces((IHost *) machine, ComSafeArrayAsOutIfaceParam(network_interfaces, IHostNetworkInterface *));
        g_pVBoxFuncs->pfnSafeArrayCopyOutIfaceParamHelper((IUnknown ***)&_network_interfaces, &network_interfaces_length, network_interfaces);
        if (network_interfaces_length > 0) {
          IHostNetworkInterface_get_IPAddress(_network_interfaces[0], &ip);
          g_pVBoxFuncs->pfnUtf16ToUtf8(ip, &cip);
          metrics.ip = cip;
          printf("ip: %s\n", metrics.ip);
        }
        // g_pVBoxFuncs->pfnUtf16ToUtf8(a, &name);
        // printf("host os name: %s\n", name);
        if (strcmp(name, "RAM/Usage/Total:avg") == 0) {
          metrics.ram_usage_total_average = res;
        }
        else if (strcmp(name, "CPU/Load/User:avg") == 0) {
          metrics.cpu_load_usage = res;
        }
      }
    }
    
    
  }

  // for (i=0; i<metric_names_length; i++) {
  //   free(names[i]);
  // }


  // free(names);
  // free(name);
  free(_data);
  free(_length);
  free(_indices);
  free(_scales);
  
  return metrics;
}

void err_exit(char *err) {
  log_err(err);
  exit(1);
}

void log_err(char *err) {
  fprintf(stderr, err);
}
