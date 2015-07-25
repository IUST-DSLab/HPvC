/**
 * Copyright (c) 2015-2016. The DSLab HPC Team
 * All rights reserved. 
 * Developers: Sina Mahmoodi, Mostafa Zamani
 * This is file we used to develop the HPvC simulator
 * */

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "msg/msg.h"            /* core library */
#include "xbt/sysdep.h"         /* calloc */
#include "xbt/synchro_core.h"

#include "xbt/log.h"
#include "xbt/asserts.h"
XBT_LOG_NEW_DEFAULT_CATEGORY(msg_test, "Messages specific for this msg example");

#define NUMBER_OF_CLUSTERS 2
// These must change if corresponding values changes in deployment.xml and cluster.xml
#define MAX_VM_MEMORY 1e9
#define MAX_PM_MEMORY 8e9
#define MAX_VM_NET 1.25e8
#define MAX_PM_NET 1.25e8
#define MAX_VM_TO_PM 50		// It is related to number of vms we run on PMs.

typedef enum HOST_ASSIGN_POLICY
{
	NONE = -1,
	INHAB_MIG = 0,
	MIN_COMM = 1
}host_assign_policy;

typedef struct slow_down_t
{
	double expected_time;
	double actual_time;
} SlowDown;

static xbt_dynar_t slow_down[NUMBER_OF_CLUSTERS];

typedef struct LoadBalanceResponse
{
	int target_host;
} load_balance_response;

typedef struct LoadBalanceRequest
{
	int request;
	int task_number;
	long double memory;
	long double net;
} load_balance_request;

typedef struct VmPlacement
{
	int vm;
	int target_pm;
} vm_placement;

typedef struct HostVmInfo
{
	int number_of_vms;
	int vms_index[MAX_VM_TO_PM];
} host_vm_info;

typedef struct ResourceNeed
{
	double cpu;
	double memory;
	double net;
	double expected_time;
	double cpu_time;
} resource_need;

static xbt_dynar_t vm_list[NUMBER_OF_CLUSTERS];
static int max_guest_job[NUMBER_OF_CLUSTERS];
static int max_pm_vm[NUMBER_OF_CLUSTERS];
static double mean_cpu_time;
static double total_cpu_time;
static double mean_arrival_time;

static xbt_dynar_t hosts_dynar;
static int number_of_involved_host; 
static int total_cluster_host;

static double cpu_max_flops;
static double memory_max;
static double net_max;

static msg_sem_t finish_sem;
static int finish;
static int migration_count;
static int migration_decision_count;

// void_f_pvoid_t function to free double*
void free_double(void* pointer)
{
	free((double*)pointer);
}

// void_f_pvoid_t finction to free host_vm_info*
void free_host_vm_info(void* pointer)
{
	free((host_vm_info*)pointer);
}

static void compute_resource_need(int rand_task, int r, int m, int c, resource_need* need)
{
	if (((double)(rand_task) / RAND_MAX) < 0.95)
	{
		r = r <= 10 ? 10 : r;
		need->cpu = (RAND_MAX / (double)(r)) * cpu_max_flops * 2;		// Based on description of opportunity cost paper
																// MAX_FLOPS = 1e9
		m = m <= 10 ? 10 : m;
		need->memory = (double)(m) / (RAND_MAX) * memory_max;		// MAX_MEM 0f each vm is 1e9

		c = c <= 10? 10 : c;
		need->net = ((double)(c) / RAND_MAX) * net_max;		// The msg_size based on cpu_need
																			// 16e6 is bandwidth in byte/sec
		need->cpu_time = RAND_MAX / (double)(r);		// We can consider virtualization impact here
		need->expected_time = need->cpu_time + need->net / MAX_VM_NET;
	}
	else 
	{
		r = r <= 10 ? 10 : r;
		need->cpu = (RAND_MAX / (double)(r)) * cpu_max_flops * 20;		// Based on description of opportunity cost paper
																// MAX_FLOPS = 1e9
		m = m <= 10 ? 10 : m;
		need->memory = (double)(m) / (RAND_MAX) * memory_max;		// MAX_MEM 0f each vm is 1e9

		c = c <= 10? 10 : c;
		need->net = ((double)(c) / RAND_MAX) * net_max;		// The msg_size based on cpu_need
																			// 16e6 is bandwidth in byte/sec
		need->cpu_time = RAND_MAX / (double)(r);		// We can consider virtualization impact here
		need->expected_time = need->cpu_time + need->net / MAX_VM_NET;
	}
}

// Create process and a task with the passing parameters of CPU, Memory and communication.
// Communication will be done with another VM that is provided by create_task process.
static int process_task(int argc, char* argv[])
{
	// Create the task with respect to parameters
	if (argc < 9)
	{
		XBT_INFO("Each process must be passed 9 arguments\n");
		return -1;
	}

	unsigned int r = atoi(argv[1]);
	unsigned int m = atoi(argv[2]);
	unsigned int c = atoi(argv[3]);
	unsigned int rand_task = atoi(argv[4]);
	unsigned int home_vm = atoi(argv[5]);
	unsigned int target_mailbox = atoi(argv[6]);
	int cluster_id = atoi(argv[7]);
	int process_sequence = atoi(argv[8]);

	resource_need need;
	compute_resource_need(rand_task, r, m, c, &need);

	//XBT_INFO("process with cpu:%f, memory: %f, net: %f on cluster: %d, vm: %d, pid: %d \
	//		cpu_time: %f, expected: %f\n",
	//		need.cpu, need.memory, need.net, cluster_id, home_vm, MSG_process_self_PID(), need.cpu_time,
	//		need.expected_time);

	char exec_name[20];
	char msg_name[20];
	sprintf(exec_name, "task_%d", MSG_process_self_PID());
	sprintf(msg_name, "msg_%d", MSG_process_self_PID());
	char target_mailbox_name[40];
	sprintf(target_mailbox_name, "mailbox_%d_%d", cluster_id, target_mailbox);
	uint8_t* data = xbt_new0(uint8_t, (uint64_t)need.memory);
	msg_task_t comm_task = MSG_task_create(msg_name, 0, need.net, NULL);

	double real_start_time = MSG_get_clock();

	// Do the task

	msg_task_t executive_task = MSG_task_create(exec_name, need.cpu, 0, data);
	MSG_task_set_data_size(executive_task, (uint64_t)need.memory);

	msg_error_t error = MSG_task_execute(executive_task);
	if (error != MSG_OK)
		XBT_INFO("Failed to execute task! %s\n", exec_name);

	int ret = MSG_OK;
	while ((ret = MSG_task_send(comm_task, target_mailbox_name)) != MSG_OK)
	{
		XBT_INFO("process_%d failed to send message to mailbox:%s\n", MSG_process_self_PID(), 
				target_mailbox_name);
	}

	double real_finish_time = MSG_get_clock();

	xbt_free(data);

	// Compute slow down of the task by dividing task_time by (real_finish_time - real_start_time)
	double actual_life_time = real_finish_time - real_start_time;

	double slowdown = actual_life_time / need.expected_time;

	//XBT_INFO("PID_%d on cluster %d is going to be off with slowdown about: %f and actual time: %f, expected: %f\n",
	//		MSG_process_self_PID(), cluster_id, slowdown, actual_life_time, need.expected_time);

	((SlowDown*)xbt_dynar_get_ptr(slow_down[cluster_id], process_sequence))->
		actual_time = actual_life_time;
	((SlowDown*)xbt_dynar_get_ptr(slow_down[cluster_id], process_sequence))->
		expected_time = need.expected_time;

	char fin_name[40];
	char fin_mailbox[20];
	sprintf(fin_name, "fin_%d", MSG_process_self_PID());
	sprintf(fin_mailbox, "fin_mailbox");
	msg_task_t fin_msg = MSG_task_create(fin_name, 0, 1, NULL);
	ret = MSG_OK;
	while ((ret = MSG_task_send(fin_msg, fin_mailbox)) != MSG_OK)
		XBT_INFO("fail to send fin message\n");


	if (cluster_id == 0)
	{
		double* mem = (double*)malloc(sizeof(double));
		*mem = - need.memory +
			*(double*)MSG_host_get_property_value(MSG_process_get_host(MSG_process_self()), "mem");
		MSG_host_set_property_value(MSG_process_get_host(MSG_process_self()),
				"mem", (char*)mem, free_double);

		double* net = (double*)malloc(sizeof(double));
		*net = - need.net +
			*(double*)MSG_host_get_property_value(MSG_process_get_host(MSG_process_self()), "net");
		MSG_host_set_property_value(MSG_process_get_host(MSG_process_self()),
				"net", (char*)net, free_double);

	//	XBT_INFO("cluster_id: %d, vm: %d mem: %f, net: %f\n", cluster_id, home_vm,
	//			*mem, *net);
	}

	return 0;
}

// Creates a processes associated with a mailbox to receive the messages from other processes
// It will not terminated. It only waits to receive some messages and then check to receive again.
// It will be killed by another process of vm or vm itself on destroy phase.
static int process_mailbox(int argc, char* argv[])
{
	if (argc < 3)
	{
		XBT_INFO("mailbox: must pass vm number and cluster number to create malbox\n");
		return -1;
	}

	int mailbox_no = atoi(argv[1]);
	int cluster_id = atoi(argv[2]);
	char mailbox_id[40];
	msg_task_t r_msg = NULL;

	sprintf(mailbox_id, "mailbox_%d_%d", cluster_id, mailbox_no);

	// We must start to listen to incoming messages asynchronously and destroy the message after getting.
	while (1)
	{
		int ret = MSG_OK;
		while ((ret = MSG_task_receive(&r_msg, mailbox_id)) != MSG_OK)
		{
			XBT_INFO("FAIL to receive on mailbox: %s\n", mailbox_id);
		}

		unsigned int data_size = MSG_task_get_data_size(r_msg);
		char* data = MSG_task_get_data(r_msg);
		if (data != NULL)
			if (!strncmp(data, "finish", data_size))
			{
				MSG_task_destroy(r_msg);
				break;
			}
		MSG_task_destroy(r_msg);
	}

	return 0;
}

// It create and runs mailbox_processes for each vm within a cluster
static void create_mailbox_processes()
{
	msg_vm_t vm = NULL;
	// Loop over vm_list and create a mailbox_process for each vm
	int cluster_id = 0;
	for (; cluster_id < NUMBER_OF_CLUSTERS; ++cluster_id)
	{
		int size_of_vm_list = xbt_dynar_length(vm_list[cluster_id]);
		int i = 0;
		for (; i < size_of_vm_list; ++i)
		{
			char process_name[40];
			char** process_argv = xbt_new(char*, 3);
			vm = xbt_dynar_get_as(vm_list[cluster_id], i, msg_vm_t);
			sprintf(process_name, "%s_mbox_%d_%d", MSG_host_get_name(vm), cluster_id, i);
			process_argv[0] = xbt_new0(char, 40);
			sprintf(process_argv[0], "%s", process_name);
			process_argv[1] = xbt_new0(char, 40);
			sprintf(process_argv[1], "%d", i);
			process_argv[2] = xbt_new0(char, 40);
			sprintf(process_argv[2], "%d", cluster_id);
			MSG_process_create_with_arguments(process_name
					, process_mailbox
					, NULL
					, vm
					, 3
					, process_argv);
		}
	}
	return;
}

// This process tries to balance the load between VMs to achieve competitive ratio of O(logN).
// It doe not know anything about how or where VMs are assigned. It uses opportunity cost approach
// to assign newly arrived jobs and terminating jobs. It can be easily implemented distributed
// as well.
// So, it takes two type of message for load balancing, ARRIVING and TERMINATING. Because the tasks
// do not change their requirements in the lifetime of process, it only reassigns on process
// TERMINATION messages
static int guest_load_balance(int argc, char* argv[])
{
	if (argc < 2)
	{
		XBT_INFO("guest load balancer: must be passed cluster_id\n");
		return -1;
	}

	int i = 0;
	int number_of_vm = 0;
	double min_cost = DBL_MAX;
	double marginal_cost = 0;
	double current_cost = 0;
	double mem_usage = 0;
	double net_usage = 0;
	msg_vm_t* vm = NULL;
	int new_load = 0;
	int vm_load = 0;
	int selected_vm_number = 0;
	int visited_vms = 0;
	int cluster_id = atoi(argv[1]);
	char load_balancer_mailbox[40];
	sprintf(load_balancer_mailbox, "load_balancer_mailbox_%d", cluster_id);
	
	msg_task_t msg_req = NULL;
	load_balance_request* request = NULL;

	msg_task_t msg_res = NULL;
	char res_name[40];
	char response_mailbox[20];
	load_balance_response* response = (load_balance_response*)malloc(sizeof(load_balance_response));

	sprintf(response_mailbox, "create_%d", cluster_id);

	// Now it waits for receiving a message from create_tasks or get_finalize processes
	// It then decide what which VM is better for the task to be created at.
	// It also gets finalize message from get_finalize and terminate itself
	// It does all communication synchronously.
	while (1)
	{
		while (MSG_task_receive(&msg_req, load_balancer_mailbox) != MSG_OK)
			XBT_INFO("load balancer %d hase failed to receive the load balancing request\n",
					cluster_id);

		// Retrieve the message request from data part and decide on what to do
		request = (load_balance_request*)MSG_task_get_data(msg_req); 

		number_of_vm = xbt_dynar_length(vm_list[cluster_id]);
		// Execute assign algorithm
		if (request->request == 0)
		{
			// TODO: Involve cpu speed
			for (i = selected_vm_number + 1, visited_vms = 0; visited_vms < number_of_vm; ++i, ++visited_vms)
			{
				i = i >= number_of_vm ? 0 : i;
				vm = (msg_vm_t*)xbt_dynar_get_ptr(vm_list[cluster_id], i);
				mem_usage = *(double*)MSG_host_get_property_value(*vm, "mem");// TODO: check if they are int or double
				net_usage = *(double*)MSG_host_get_property_value(*vm, "net");// TODO: check if they are int or double
				vm_load = xbt_swag_size(MSG_host_get_process_list(*vm));

				int ncpus = MSG_host_get_core_number(*vm);
				// Compute marginal cost
				marginal_cost =
							pow(number_of_vm, ((double)(vm_load + 1) / (ncpus * max_guest_job[cluster_id]))) +
							pow(number_of_vm, (mem_usage + request->memory) / MAX_VM_MEMORY) +
							pow(number_of_vm, (net_usage + request->net) / MAX_VM_NET) -
							pow(number_of_vm, (double)(vm_load) / (ncpus * max_guest_job[cluster_id])) -
							pow(number_of_vm, mem_usage / MAX_VM_MEMORY) -
							pow(number_of_vm, net_usage / MAX_VM_NET);

				//XBT_INFO("marginal cost on cluster_id: %d on vm :%d for task: %d is %f\n",
				//		cluster_id, i, request->task_number, marginal_cost);

				if (marginal_cost < min_cost)
				{
					min_cost = marginal_cost;
					//XBT_INFO("change selected vm:%d into vm:%d\n", selected_vm_number, i);
					selected_vm_number = i;
					new_load = vm_load + 1;
				}
			}
			
			if (new_load >= max_guest_job[cluster_id])
				max_guest_job[cluster_id]*= 2;

		//	XBT_INFO("load balance on cluster %d , home %d for task %d marginal cost %f\n",
		//			cluster_id, selected_vm_number, request->task_number, min_cost);
			response->target_host = selected_vm_number;
			sprintf(res_name, "res_%d_%d", cluster_id, request->task_number);
			msg_res = MSG_task_create(res_name, 0, sizeof(*response), response);
			while (MSG_task_send(msg_res, response_mailbox) != MSG_OK)
				XBT_INFO("Load Balancer has just failed to response the request for %d on %d",
						request->task_number, cluster_id);
			min_cost = DBL_MAX;
			//selected_vm_number = 0;
			new_load = 0;
		}
		// Execute reassign algorithm
		else if (request->request == 1)
		{

		}
		else
		{
			MSG_task_destroy(msg_req);
			msg_req = NULL;
			break;
		}
		MSG_task_destroy(msg_req);
		msg_req = NULL;
	}

	return 0;
}

// TODO: Implement the reorganize according to communication overhead
static int policy_func_min_comm(int cluster_id)
{
	return 0;
}

// Place a VM and update the host_vm_info of the target pm
static void place_vm(int cluster_id, vm_placement placement)
{
	msg_host_t host = *((msg_host_t*)xbt_dynar_get_ptr(hosts_dynar, placement.target_pm));
	host_vm_info pre_hvi = *(host_vm_info*)MSG_host_get_property_value(host, "host_vm_info");

	// It can be logged if target has no further place for new vms
	if (pre_hvi.number_of_vms >= MAX_VM_TO_PM)
		return;

	msg_vm_t vm = *((msg_vm_t*)xbt_dynar_get_ptr(vm_list[cluster_id], placement.vm));

	// Getting previous host of migrant vm
	msg_host_t pre_host = MSG_vm_get_pm(vm);

	// Doing Migration *********************
	MSG_vm_migrate(vm, host);

	migration_count++;

	// Update new host info
	host_vm_info* new_hvi = (host_vm_info*)malloc(sizeof(host_vm_info));
	memcpy(new_hvi, &pre_hvi, sizeof(host_vm_info));
	new_hvi->vms_index[new_hvi->number_of_vms] = placement.target_pm;
	new_hvi->number_of_vms++;
	MSG_host_set_property_value(host, "host_vm_info", (char*)new_hvi, free_host_vm_info);

	// Update previous host info
	host_vm_info pre_host_hvi = *(host_vm_info*)MSG_host_get_property_value(pre_host, "host_vm_info");
	host_vm_info* pre_host_new_hvi = (host_vm_info*)malloc(sizeof(host_vm_info));
	memcpy(pre_host_new_hvi, &pre_host_hvi, sizeof(host_vm_info));
	pre_host_new_hvi->number_of_vms--;
	MSG_host_set_property_value(pre_host, "host_vm_info", (char*)pre_host_new_hvi, free_host_vm_info);

	if (new_hvi->number_of_vms >= max_pm_vm[cluster_id])
		max_pm_vm[cluster_id] *= 2;		// TODO: Use "K" instead of "1"
}

/**
 * It is based on the policy of evaluating cost of migration against cost inhabitancy to make decision
 * on where to migrate VMs.
 * It is based on opportunity cost model and takes into account the migration cost as a task with
 * communication cost, memory and CPU overhead.
 * It checks the stability condition of opportunity cost model and replace VMs to improve performance.
 * */
static int policy_func_inhab_mig(int cluster_id)
{
	int M = number_of_involved_host;
	int m = 0, vm = 0, tvm = 0, k = 0;
	vm_placement placement;
	memset(&placement, -1, sizeof(placement));
	double inhabitancy_cost = 0, migration_cost = 0;
	double min_cost = DBL_MAX;

	msg_vm_t current_vm = NULL;
	msg_vm_t target_vm = NULL;
	msg_host_t current_host = NULL;
	msg_host_t target_host = NULL;
	host_vm_info* hvi = NULL;
	host_vm_info* thvi = NULL;

	double pm_memory_usage = 0;
	double pm_net_usage = 0;
	int pm_load = 0;
	double vm_memory_usage = 0;
	double vm_net_usage = 0;
	int vm_load = 0;
	double tm_memory_usage = 0;
	double tm_net_usage = 0;
	int tm_load = 0;

	// Find boundary of PMs in the cluster with cluster_id
	int host = (cluster_id) * total_cluster_host;
	M = host + number_of_involved_host;

	while (1)
	{
		MSG_process_sleep(10);		// TODO: It can be adjust to arrival rate or process lifetime

		MSG_sem_acquire(finish_sem);
		if (finish)
		{
			MSG_sem_release(finish_sem);
			break;
		}

		for (m = host; m < M; ++m)
		{
			current_host = *((msg_host_t*)xbt_dynar_get_ptr(hosts_dynar, m));
			hvi = (host_vm_info*)MSG_host_get_property_value(current_host, "host_vm_info");

			int ncpus_host = MSG_host_get_core_number(current_host);
			// Initialize the PM load indices
			pm_memory_usage = 0;
			pm_net_usage = 0;
			pm_load = 0;
			for (vm = 0; vm < hvi->number_of_vms; ++vm)
			{
				current_vm = *((msg_vm_t*)xbt_dynar_get_ptr(vm_list[cluster_id], hvi->vms_index[vm]));
				int ncpus = MSG_host_get_core_number(current_vm);
				pm_memory_usage += *(double*)MSG_host_get_property_value(current_vm, "mem");
				pm_net_usage += *(double*)MSG_host_get_property_value(current_vm, "net");
				pm_load += xbt_swag_size(MSG_host_get_process_list(current_vm)) / (double)ncpus;
			}

			int done_migration = 0;
			for (vm = 0; vm < hvi->number_of_vms; ++vm)
			{
				current_vm = *((msg_vm_t*)xbt_dynar_get_ptr(vm_list[cluster_id], hvi->vms_index[vm]));
				//int ncpus = MSG_host_get_core_number(current_vm);
				vm_memory_usage = *(double*)MSG_host_get_property_value(current_vm, "mem");
				vm_net_usage = *(double*)MSG_host_get_property_value(current_vm, "net");
				//vm_load = xbt_swag_size(MSG_host_get_process_list(current_vm)) / (double)ncpus;
				vm_load = xbt_swag_size(MSG_host_get_process_list(current_vm));

				inhabitancy_cost =
					pow(number_of_involved_host, ((double)pm_load)/(max_pm_vm[cluster_id] * ncpus_host)) +
					//pow(number_of_involved_host, pm_memory_usage/MAX_PM_MEMORY) +
					pow(number_of_involved_host, pm_net_usage/MAX_PM_NET) -
					pow(number_of_involved_host, ((double)(pm_load - vm_load))/(max_pm_vm[cluster_id] * ncpus_host)) -
					//pow(number_of_involved_host, (pm_memory_usage - vm_memory_usage)/MAX_PM_MEMORY) -
					pow(number_of_involved_host, (pm_net_usage - vm_net_usage)/MAX_PM_NET);

				for (k = host; k < M; ++k)		// TODO: Selecting from a small set
				{
					// Checking the condition based on other pms
					if (k == m)	continue;

					// Initialize the target PM load indices
					target_host = *((msg_host_t*)xbt_dynar_get_ptr(hosts_dynar, k));
					thvi = (host_vm_info*)MSG_host_get_property_value(target_host, "host_vm_info");
					int tncpus = MSG_host_get_core_number(target_host);
					tm_memory_usage = 0;
					tm_net_usage = 0;
					tm_load = 0;
					for (tvm = 0; tvm < thvi->number_of_vms; ++tvm)
					{
						target_vm = *((msg_vm_t*)xbt_dynar_get_ptr(vm_list[cluster_id], thvi->vms_index[tvm]));
						//int tncpus = MSG_host_get_core_number(current_vm);
						tm_memory_usage += *(double*)MSG_host_get_property_value(target_vm, "mem");
						tm_net_usage += *(double*)MSG_host_get_property_value(target_vm, "net");
						//tm_load += xbt_swag_size(MSG_host_get_process_list(target_vm)) / (double)tncpus;
						tm_load += xbt_swag_size(MSG_host_get_process_list(target_vm));
					}

					migration_cost =
						pow(number_of_involved_host, ((double)(tm_load + vm_load))/(max_pm_vm[cluster_id] * tncpus)) +
						//pow(number_of_involved_host, (tm_memory_usage + vm_memory_usage)/MAX_PM_MEMORY) +
						pow(number_of_involved_host, (tm_net_usage + vm_net_usage)/MAX_PM_NET) -
						pow(number_of_involved_host, ((double)tm_load)/(max_pm_vm[cluster_id] * tncpus)) -
						//pow(number_of_involved_host, tm_memory_usage/MAX_PM_MEMORY) -
						pow(number_of_involved_host, tm_net_usage/MAX_PM_NET);

					// comparing costs
					if (migration_cost <= inhabitancy_cost / 2)
					//if (migration_cost < inhabitancy_cost)
					{
					//	if (migration_cost < min_cost)
					//	{
							migration_decision_count++;
					//		min_cost = migration_cost;
							placement.vm = hvi->vms_index[vm];
							placement.target_pm = k;
							place_vm(cluster_id, placement);
							memset(&placement, -1, sizeof(vm_placement));
							done_migration = 1;
							break;
					//	}
					}
				}

				if (done_migration)
					break;
			}

			// A new placement is selected
		//	if (placement.vm > -1 && placement.target_pm > -1)
		//	{
		//		place_vm(cluster_id, placement);
		//		memset(&placement, -1, sizeof(vm_placement));
		//	}
		}

		MSG_sem_release(finish_sem);
	}

	return 0;
}

static int host_load_balance(int argc, char* argv[])
{
	if (argc < 3)
	{
		XBT_INFO("host load balancer: must be passed cluster_id assign policy\n");
		return -1;
	}

	int cluster_id = atoi(argv[1]);
	host_assign_policy policy = atoi(argv[2]);

	// A loop that checks every K (default is 60) seconds, the load imbalance of hosts of cluster to make decision
	// on reassignment based on the given policy.
	// The time of sleep is depends on estimated vm migration time.
	// It will be terminated on receiving finish message perhaps!
	switch (policy)
	{
		case NONE:
			{
				XBT_INFO("NO VM Replacement policy is enabled\n");
				break;
			}
		case INHAB_MIG:
			{
				XBT_INFO("Calling Inhabitancy migration policy\n");
				// Going to infinite loop
				policy_func_inhab_mig(cluster_id);
				break;
			}
		case MIN_COMM:
			{
				XBT_INFO("Calling Minimization Communication policy\n");
				break;
			}
		default:
			return -2;
	};

	return 0;
}

// Creates tasks and applies them to a machine.
// To be same for all executions, we must consider two separate clusters that the created tasks will submit in
// the order.
static int create_tasks(int argc, char* argv[])
{
	if (argc < 5)
	{
		XBT_INFO("Must pass the number of process to be simulated,\
				the seed of random numbers, number of VMs and rate of task arrival\n");
		return -1;
	}

	int number_of_processes = atoi(argv[1]);
	unsigned time_seed = atoi(argv[2]);
	int number_of_vms = atoi(argv[3]);
	double process_arrival_rate = atof(argv[4]);

	create_mailbox_processes();

	srand(time_seed);

	double arrival = 0;
	unsigned int r = 0; // the factor we use in jobs execution time
	unsigned int m = 0;
	unsigned int c = 0;
	unsigned int rand_task = 0;
	unsigned int home_vm = 0;
	unsigned int target_mailbox = 0;

	msg_process_t process = NULL;
	int process_argc = 9;
	msg_vm_t host = NULL;
	
	resource_need need;
	msg_task_t msg_req = NULL;
	char req_name[NUMBER_OF_CLUSTERS][40];
	char load_balancer_mailbox[NUMBER_OF_CLUSTERS][40];
	load_balance_request* request = (load_balance_request*)malloc(sizeof(load_balance_request));
	msg_task_t msg_res = NULL;
	char response_mailbox[NUMBER_OF_CLUSTERS][20];
	//load_balance_response* response = NULL;

	int cluster_id = 0;
	for (; cluster_id < NUMBER_OF_CLUSTERS; ++cluster_id)
	{
		sprintf(response_mailbox[cluster_id], "create_%d", cluster_id);
		sprintf(load_balancer_mailbox[cluster_id], "load_balancer_mailbox_%d", cluster_id);
	}

	// Loops until creates all processes. We think that number of processes must be larger than 10000.
	// 5% of tasks must be of 20/r CPU and 95% must be 2/r.
	// The communication time is c% of CPU time.
	int i = 0;
	for (; i < number_of_processes; ++i)
	{
	//	if ( i % (20 % (number_of_processes / 20)) == 0)
	//	{
			arrival = ((double)rand()) / RAND_MAX;
			arrival = arrival == 1 ? arrival - 0.005 : arrival;
			double sleep_time = -log(1 - arrival) / process_arrival_rate;
			mean_arrival_time += sleep_time / number_of_processes;
			MSG_process_sleep(sleep_time);
	//	}

		r = rand();
		srand(r);
		m = rand();
		srand(m);
		c = rand();
		srand(c);
		rand_task = rand();
		srand(rand_task);
		target_mailbox = rand();

		compute_resource_need(rand_task, r, m, c, &need);
		mean_cpu_time += (need.cpu / cpu_max_flops) / number_of_processes;
		total_cpu_time += need.cpu / cpu_max_flops;

		for ( cluster_id = 0; cluster_id < NUMBER_OF_CLUSTERS; ++cluster_id)
		{
			// Only first cluster does load balance
			if (cluster_id == 0)
			{
				//rand();

				request->memory = need.memory;
				request->net = need.memory;
				request->task_number = i;
				request->request = 0;

				sprintf(req_name[cluster_id], "load_balancing_%d_%d", cluster_id, i);
				msg_req = MSG_task_create(req_name[cluster_id], 0, sizeof(*request), request);

				// Request-Response to load balancer
				while (MSG_task_send(msg_req, load_balancer_mailbox[cluster_id]) != MSG_OK)
					XBT_INFO("create task has just failed to send load balancing request of %d on %d\n", i,
							cluster_id);

				while (MSG_task_receive(&msg_res, response_mailbox[cluster_id]) != MSG_OK)
					XBT_INFO("create task has just failed to receive load balancing response of %d on %d\n",
							i, cluster_id);

				home_vm = ((load_balance_response*)MSG_task_get_data(msg_res))->target_host;
				//XBT_INFO("cluster_id: %d , home vm: %d", cluster_id, home_vm);
			}
			else
			{
				home_vm = i % number_of_vms;		// Round-Robin
				//home_vm = rand() % number_of_vms;
				//XBT_INFO("cluster_id: %d , home vm: %d", cluster_id, home_vm);
			}

			char process_name[40];
			char** process_argv = xbt_new(char*, 9);
			sprintf(process_name, "process_%d_%d", cluster_id, i);
			process_argv[0] = xbt_new0(char, 40);
			sprintf(process_argv[0], "%s", process_name);
			process_argv[1] = xbt_new0(char, 40);
			sprintf(process_argv[1], "%u", r);
			process_argv[2] = xbt_new0(char, 40);
			sprintf(process_argv[2], "%u", m);
			process_argv[3] = xbt_new0(char, 40);
			sprintf(process_argv[3], "%u", c);
			process_argv[4] = xbt_new0(char, 40);
			sprintf(process_argv[4], "%u", rand_task);
			process_argv[5] = xbt_new0(char, 40);
			sprintf(process_argv[5], "%u", home_vm % number_of_vms);
			process_argv[6] = xbt_new0(char, 40);
			sprintf(process_argv[6], "%u", target_mailbox % number_of_vms);
			process_argv[7] = xbt_new0(char, 40);
			sprintf(process_argv[7], "%d", cluster_id);
			process_argv[8] = xbt_new0(char, 40);
			sprintf(process_argv[8], "%d", i);
			
			host = xbt_dynar_get_as(vm_list[cluster_id], home_vm, msg_vm_t);

			process = MSG_process_create_with_arguments( process_name
													   , process_task
													   , NULL
													   , host
													   , process_argc
													   , process_argv);

			// TODO: Adding resource consumption to vm  property and make the laod balancer 
			// robust against its null value

			if (cluster_id == 0)
			{
				double* mem = (double*)malloc(sizeof(double));
				*mem = need.memory + *(double*)MSG_host_get_property_value(host, "mem");
				MSG_host_set_property_value(host, "mem", (char*)mem, free_double);

				double* net = (double*)malloc(sizeof(double));
				*net = need.net + *(double*)MSG_host_get_property_value(host, "net");
				MSG_host_set_property_value(host, "net", (char*)net, free_double);

			//	XBT_INFO("cluster_id: %d, vm: %d mem: %f, net: %f\n", cluster_id, home_vm,
			//			*mem, *net);

				MSG_task_destroy(msg_res);
			}
		}
	}

	return 0;
}

// It waits for incoming messages from all processes that says they have already been finished
// Afterwards, it sends all vm listeners a message to finish listening
static int get_finalize(int argc, char* argv[])
{
	if (argc < 3)
	{
		XBT_INFO("get finalize must be passes number of processes and vms\n");
		return -1;
	}

	int number_of_processes = atoi(argv[1]);
	int number_of_vms = atoi(argv[2]);

	char fin_mailbox[20];
	sprintf(fin_mailbox, "fin_mailbox");

	msg_task_t r_msg = NULL;


	// We must start to listen to incoming messages asynchronously and destroy the message after getting.
	int i = 0;
	for (; i < NUMBER_OF_CLUSTERS * number_of_processes; ++i)
	{
		int ret = MSG_OK;
		while ((ret = MSG_task_receive(&r_msg, fin_mailbox) != MSG_OK))
			XBT_INFO("faile to receive the fin from process\n");

		MSG_task_destroy(r_msg);
		r_msg = NULL;
	}

	// We are sure about finalization of all processes
	MSG_process_sleep(10);

	// Send finish message to all vm listener
	char vm_mailbox[40];
	char fin_name[40];
	char load_balancer_mailbox[40];
	msg_task_t fin_msg = NULL;
	int cluster_id = 0;
	for (; cluster_id < NUMBER_OF_CLUSTERS; ++cluster_id)
	{
		for (i = 0; i < number_of_vms; ++i)
		{
			sprintf(vm_mailbox, "mailbox_%d_%d", cluster_id, i);
			sprintf(fin_name, "finish_%d_%d", cluster_id, i);
			fin_msg = MSG_task_create(fin_name, 0, 6, "finish");
			int ret = MSG_OK;
			while((ret = MSG_task_send(fin_msg, vm_mailbox)) != MSG_OK)
				XBT_INFO("FAILURE in sending fin to VMs\n");
		}

		if (cluster_id == 0)
		{
			load_balance_request* fin_request = (load_balance_request*)malloc(sizeof(load_balance_request));
			fin_request->request = 2;
			sprintf(fin_name, "finish_%d", cluster_id);
			fin_msg = MSG_task_create(fin_name, 0, 6, fin_request);
			sprintf(load_balancer_mailbox, "load_balancer_mailbox_%d", cluster_id);
			while (MSG_task_send(fin_msg, load_balancer_mailbox) != MSG_OK)
				XBT_INFO("get finalize has just failed to send fin to load balancer\n");
		}
	}

	MSG_process_sleep(10);

	MSG_sem_acquire(finish_sem);
	finish = 1;
	MSG_sem_release(finish_sem);

	return 0;
}

// Put this two functions in a process to be sync with other parts
// We assume that hosts are in a dynar based on their number and cluster declaration in cluster.xml
static void launch_master(unsigned no_vm, int no_process, int cluster_id, host_assign_policy policy, int random)
{
	finish_sem = MSG_sem_init(1);
	finish = 0;
	migration_count = 0;
	migration_decision_count = 0;

	slow_down[cluster_id] = xbt_dynar_new(sizeof(SlowDown), NULL);
	int i = 0, j = 0, k = 0;
	for (; i < no_process; ++i)
	{
		SlowDown slow_down_process;
		memset(&slow_down_process, 0, sizeof(slow_down_process));
		xbt_dynar_set(slow_down[cluster_id], i, &slow_down_process);
	}

	vm_list[cluster_id] = xbt_dynar_new(sizeof(msg_vm_t), NULL);

	// Number of VM per host
	int vm_to_host = no_vm / number_of_involved_host;
	max_pm_vm[cluster_id] = vm_to_host + 1;

	// The last is the one who dose management for this step
	int host = (cluster_id) * total_cluster_host;
	int max_host= host + no_vm / number_of_involved_host;

	msg_host_t pm = NULL;

	char vm_name[20];

	long ramsize = 1L * 256 * 1000 * 1000;
	memory_max = ramsize / 10000;
	net_max = MAX_VM_NET / vm_to_host;
	cpu_max_flops = MSG_get_host_speed(*((msg_host_t*)xbt_dynar_get_ptr(MSG_hosts_as_dynar(), 0)));


	for (j = host; j < host + number_of_involved_host; j++, k++)
	{
		pm = xbt_dynar_get_as(hosts_dynar, j, msg_host_t);
		host_vm_info* hvi = (host_vm_info*)malloc(sizeof(host_vm_info));
		memset(hvi, 0, sizeof(host_vm_info));

		for (i = k * vm_to_host; i < (k + 1) * vm_to_host && i < no_vm; ++i)
		{
			sprintf(vm_name, "vm_%d_%d", cluster_id, i);

			s_ws_params_t params;
			memset(&params, 0, sizeof(params));
			params.ramsize = ramsize;
		//	if (i % (no_vm / 50) == random)
		//		params.ncpus = 4;

			msg_vm_t vm = MSG_vm_create_core(pm, vm_name);
			MSG_host_set_params(vm, &params);

			xbt_dynar_set(vm_list[cluster_id], i, &vm);

			// TODO: check if need to do it with strings
			double* mem = (double*)malloc(sizeof(double));
			*mem = 0.0;
			MSG_host_set_property_value(vm, "mem", (char*)mem, free_double);

			double* net = (double*)malloc(sizeof(double));
			*net = 0.0;
			MSG_host_set_property_value(vm, "net", (char*)net, free_double);

			hvi->vms_index[hvi->number_of_vms] = i;
			hvi->number_of_vms++;

			MSG_vm_start(vm);
		}
		MSG_host_set_property_value(pm, "host_vm_info", (char*)hvi, free_host_vm_info);
	}

	if (cluster_id == 0)
	{
		// Creating guest load balancer process
		int glb_process_argc = 2;
		char glb_process_name[40];
		char** glb_process_argv = xbt_new(char*, 2);
		sprintf(glb_process_name, "guest_load_balancer_%d", cluster_id);
		glb_process_argv[0] = xbt_new0(char, 40);
		sprintf(glb_process_argv[0], "%s", glb_process_name);
		glb_process_argv[1] = xbt_new0(char, 40);
		sprintf(glb_process_argv[1], "%d", cluster_id);
		msg_process_t glb_load_balancer = MSG_process_create_with_arguments(glb_process_name,
				guest_load_balance,
				NULL,
				xbt_dynar_get_as(hosts_dynar, host, msg_host_t),
				glb_process_argc,
				glb_process_argv);

		// Creating host load balancer process
		
		int hlb_process_argc = 3;
		char hlb_process_name[40];
		char** hlb_process_argv = xbt_new(char*, 3);
		sprintf(hlb_process_name, "host_load_balancer_%d", cluster_id);
		hlb_process_argv[0] = xbt_new0(char, 40);
		sprintf(hlb_process_argv[0], "%s", hlb_process_name);
		hlb_process_argv[1] = xbt_new0(char, 40);
		sprintf(hlb_process_argv[1], "%d", cluster_id);
		hlb_process_argv[2] = xbt_new0(char, 40);
		sprintf(hlb_process_argv[2], "%d", policy);
		msg_process_t hlb_load_balancer = MSG_process_create_with_arguments(hlb_process_name,
				host_load_balance,
				NULL,
				xbt_dynar_get_as(hosts_dynar, host, msg_host_t),
				hlb_process_argc,
				hlb_process_argv);

	}
}

static void destroy_master(unsigned no_vm, int no_process, int cluster_id)
{
	msg_vm_t vm;

	int i = 0;
	while (!xbt_dynar_is_empty(vm_list[cluster_id]))
	{
		++i;

		xbt_dynar_remove_at(vm_list[cluster_id], 0, &vm);

		if (MSG_vm_is_running(vm))
			MSG_vm_shutdown(vm);

		if (MSG_vm_is_suspended(vm))
		{
			MSG_vm_resume(vm);
			MSG_vm_shutdown(vm);
		}

		MSG_vm_destroy(vm);
	}

	xbt_dynar_free(&vm_list[cluster_id]);

	double mean_slowdown = 0;
	double slowdown, actual, expected;
	for (i = 0; i < no_process; ++i)
	{
		actual = ((SlowDown*)xbt_dynar_get_ptr(slow_down[cluster_id], i))->actual_time;
		expected = ((SlowDown*)xbt_dynar_get_ptr(slow_down[cluster_id], i))->expected_time;

		slowdown = actual / expected;

	//	printf("process: %d, slow down: %f, actual time: %f, expected time: %f\n",
	//			i, slowdown, actual, expected);

		mean_slowdown += slowdown;
	}

	mean_slowdown/=no_process;
	printf("cluster: %d slow down is : %f\n", cluster_id, mean_slowdown);
	printf("number of migration is : %d\n", migration_count);
	printf("number of migration decision: %d\n", migration_decision_count);
	printf("mean cpu time need: %f, total cpu time: %f, mean_arrival_time: %f\n",
			mean_cpu_time, total_cpu_time, mean_arrival_time);
	// Use information before free
	xbt_dynar_remove_n_at(slow_down[cluster_id], no_process, 0);
	xbt_dynar_free(&slow_down[cluster_id]);
}

int main(int argc, char *argv[])
{
	msg_error_t res = MSG_OK;

	if (argc < 6)
	{
		XBT_CRITICAL("Usage: %s cluster.xml deployment.xml <number of VMs per cluster> <number of processes> <host assign policy>\n", argv[0]);
		exit(1);
	}
	/* Argument checking */
	MSG_init(&argc, argv);
	MSG_create_environment(argv[1]);
	MSG_function_register("create_tasks", create_tasks);
	MSG_function_register("process_task", process_task);
	MSG_function_register("process_mailbox", process_mailbox);
	MSG_function_register("get_finalize", get_finalize);
	MSG_function_register("guest_load_balance", guest_load_balance);
	MSG_function_register("host_load_balance", host_load_balance);

	hosts_dynar = MSG_hosts_as_dynar();
	number_of_involved_host = (xbt_dynar_length(hosts_dynar) - 1) / NUMBER_OF_CLUSTERS - 1;
	total_cluster_host = (xbt_dynar_length(hosts_dynar) - 1) / NUMBER_OF_CLUSTERS;
	mean_cpu_time = 0;
	total_cpu_time = 0;
	mean_arrival_time = 0;

	srand(time(NULL));
	int random = rand() % (atoi(argv[3]) / 50);

	int i = 0;
	for (; i < NUMBER_OF_CLUSTERS; ++i)
	{
		vm_list[i] = NULL;
		max_guest_job[i] = 1;
		launch_master(atoi(argv[3]), atoi(argv[4]), i, atoi(argv[5]), random);
	}

	MSG_launch_application(argv[2]);

	// Main start-point of simulation
	res = MSG_main();

	for (i = 0; i < NUMBER_OF_CLUSTERS; ++i)
	{
		destroy_master(atoi(argv[3]), atoi(argv[4]), i);
		vm_list[i] = NULL;
	}

	XBT_INFO("Simulation time %g\n", MSG_get_clock());

	if (res == MSG_OK)
		return 0;
	else
		return 1;
}
