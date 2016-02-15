/**
 * Copyright (c) 2015-2016. The DSLab HPC Team
 * All rights reserved. 
 * Developers: Sina Mahmoodi, Mostafa Zamani
 * This is file we used to develop the HPvC simulator
 * 
 * Documentation of SimGrid Simulator
 * http://simgrid.gforge.inria.fr/simgrid/latest/doc/
 * */

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>				// for using INT_MAX

#include "utility.h"			// some utility fonction such as RemoveItemFromArray
#include "structures.h"


#include "simgrid/msg.h"        /* core library */
#include "xbt/sysdep.h"         /* calloc */
#include "xbt/synchro_core.h"

#include "xbt/log.h"
#include "xbt/asserts.h"
XBT_LOG_NEW_DEFAULT_CATEGORY(msg_test, "Messages specific for this msg example");


const int INITIAL_TRANMISSION_RATE = 1;
const float INITIAL_TRANMISSION_LATENCY = 0.006f; // it's the minimum and defalt latency based on curronet toppology 

const float COMP_PER_COMM = 0.2;			// this is computation per communication rate!
const float ITERATION_PER_PROCESS_RATIO = 0.5;
int ITERATION_PER_PROCESS ;			// NUMBER_OF_VMS * ITERATION_PER_PROCESS_RATIO
int COMP_TASK_SIZE;			// COMP_PER_COMM * ITERATION_PER_PROCESS
int COMM_TASK_SIZE;			// ITERATION_PER_PROCESS - COMP_TASK_SIZE


static int NUMBER_OF_VMS;
static int NUMBER_OF_PROCESS;
static int NUMBER_OF_INVOLVED_HOST;
static float VM_DOWN_TIME_SERVICE_SAME_CLUSTER; // seconds
static float VM_DOWN_TIME_SERVICE_OTHER_CLUSTER; // seconds

static int migrate_counter = 0;
static msg_process_t ORGANIZER_PROCESS;
static int is_organizer_running = 0;


static xbt_dynar_t dataOfPMs[NUMBER_OF_CLUSTERS];

int **transmissionRate;
float **transmissionLatency;


static xbt_dynar_t slow_down[NUMBER_OF_CLUSTERS];

static xbt_dynar_t vm_list[NUMBER_OF_CLUSTERS];
static int max_guest_job[NUMBER_OF_CLUSTERS];

static xbt_dynar_t hosts_dynar;

static double cpu_max_flops;
static double memory_max;
static double net_max;



// declare functions signature
int send_task( msg_task_t task, const char *mailBoxName,msg_vm_t vmSender, msg_vm_t vmReceiver);
void migrate_vm(msg_vm_t vm,int VM_Index,msg_host_t targetPM,int targetPM_Index);

// void_f_pvoid_t function to free double*
void free_double(void* pointer)
{
	free((double*)pointer);
}

static void compute_resource_need(int rand_task, int r, int m, int c, resource_need* need)
{
	if (((double)(rand_task) / RAND_MAX) < 1.f)
	{
		//r = r <= 10 ? 10 : r;
		// need->cpu = (RAND_MAX / (double)(r)) * cpu_max_flops;		// Based on description of opportunity cost paper
		need->cpu = 0.1f * cpu_max_flops;			// MAX_FLOPS =  12* 2e9

		m = m <= 10 ? 10 : m;
		need->memory = (double)(m) / (RAND_MAX) * memory_max;		// MAX_MEM 0f each vm is 1e9

		//c = c <= 10? 10 : c;
		need->net = 0.4f * net_max;		// The msg_size based on cpu_need
																			// 16e6 is bandwidth in byte/sec
		need->cpu_time = RAND_MAX / (double)(r);		// We can consider virtualization impact here
		need->expected_time = ( need->cpu_time) + ( (need->net / MAX_VM_NET) );
	}
	else
	{
		r = r <= 10 ? 10 : r;
		//need->cpu = (RAND_MAX / (double)(r)) * cpu_max_flops * 10;		// Based on description of opportunity cost paper
		need->cpu = ( (double)(r) / RAND_MAX ) * cpu_max_flops * 10;		// MAX_FLOPS = 12* 1e9
																	
		m = m <= 10 ? 10 : m;
		need->memory = (double)(m) / (RAND_MAX) * memory_max;		// MAX_MEM 0f each vm is 1e9

		//c = c <= 10? 10 : c;
		need->net = 0.03f * net_max;		// The msg_size based on cpu_need
																			// 16e6 is bandwidth in byte/sec
		need->cpu_time = (RAND_MAX / (double)(r)) * 10;		// We can consider virtualization impact here
		need->expected_time = ( need->cpu_time * COMP_TASK_SIZE ) + ( (need->net / MAX_VM_NET) * COMM_TASK_SIZE );
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


	/*XBT_INFO("process with cpu:%f, memory: %f, net: %f on cluster: %d, vm: %d, pid: %d \
			cpu_time: %f, expected: %f\n",
			need.cpu, need.memory, need.net, cluster_id, home_vm, MSG_process_self_PID(), need.cpu_time,
			need.expected_time); */

	char exec_name[20];
	char msg_name[20];
	sprintf(exec_name, "task_%d", MSG_process_self_PID());
	sprintf(msg_name, "msg_%d", MSG_process_self_PID());
	char target_mailbox_name[40];
	uint8_t* data = xbt_new0(uint8_t, (uint64_t)need.memory);
	

	double real_start_time = MSG_get_clock();

	// Do the task
	int i = 0;
	int n = 0 , p = 0;
	while(i < ITERATION_PER_PROCESS )
	{
		if( p < COMP_TASK_SIZE )
		{
			msg_task_t executive_task = MSG_task_create(exec_name, need.cpu, 0, data);
			MSG_task_set_data(executive_task, (uint64_t)need.memory);

			msg_error_t error = MSG_task_execute(executive_task);
			if (error != MSG_OK)
				XBT_INFO("Failed to execute task! %s\n", exec_name);
			p++;
			i++;
		}
		if( n < COMM_TASK_SIZE && i < ITERATION_PER_PROCESS )
		{
			msg_task_t comm_task = MSG_task_create(msg_name, 0, need.net, NULL);
			int ret = MSG_OK;

			msg_vm_t vmSender,vmReceiver;
			vmSender = xbt_dynar_get_as(vm_list[cluster_id],home_vm, msg_vm_t);

			if( i%2 )
			{
				vmReceiver = xbt_dynar_get_as(vm_list[cluster_id],target_mailbox, msg_vm_t);
			}
			else
			{
				target_mailbox =  GetNextTargetMailBox(target_mailbox,process_sequence*n,NUMBER_OF_VMS);
				vmReceiver = xbt_dynar_get_as(vm_list[cluster_id],target_mailbox, msg_vm_t);
			}

			sprintf(target_mailbox_name, "mailbox_%d_%d", cluster_id, target_mailbox);
			while ( (ret = send_task(comm_task, target_mailbox_name,vmSender,vmReceiver)) != MSG_OK)
			{
				XBT_INFO("process_%d failed to send message to mailbox:%s\n", MSG_process_self_PID(), 
						target_mailbox_name);
			}
			n++;
			i++;
		}
	}

	// msg_task_t executive_task = MSG_task_create(exec_name, need.cpu, 0, data);
	// MSG_task_set_data(executive_task, (uint64_t)need.memory);

	// msg_error_t error = MSG_task_execute(executive_task);
	// if (error != MSG_OK)
	// 	XBT_INFO("Failed to execute task! %s\n", exec_name);

	// int ret = MSG_OK;
	// // increase number of send msg to 10 -> 50% to target_mailbox and 50% to random mailbox
	// int i = 0;
	// int NUMBER_OF_COMM_MSG = 20;
	// for( i = 0;i< NUMBER_OF_COMM_MSG;i++)
	// {
	// 	msg_task_t comm_task = MSG_task_create(msg_name, 0, need.net, NULL);
	// 	ret = MSG_OK;

	// 	msg_vm_t vmSender,vmReceiver;
	// 	vmSender = xbt_dynar_get_as(vm_list[cluster_id],home_vm, msg_vm_t);

	// 	if( i%2 )
	// 	{
	// 		vmReceiver = xbt_dynar_get_as(vm_list[cluster_id],target_mailbox, msg_vm_t);
	// 	}
	// 	else
	// 	{
	// 		target_mailbox =  ( target_mailbox + 10 ) % NUMBER_OF_VMS;
	// 		vmReceiver = xbt_dynar_get_as(vm_list[cluster_id],target_mailbox, msg_vm_t);
	// 	}

	// 	sprintf(target_mailbox_name, "mailbox_%d_%d", cluster_id, target_mailbox);
	// 	while ((ret = send_task(comm_task, target_mailbox_name,vmSender,vmReceiver)) != MSG_OK)
	// 	{
	// 		XBT_INFO("process_%d failed to send message to mailbox:%s\n", MSG_process_self_PID(), 
	// 				target_mailbox_name);
	// 	}
	// }
	

	// while ((ret = MSG_task_send(comm_task,target_mailbox_name)) != MSG_OK)
	// {
	// 	XBT_INFO("process_%d failed to send message to mailbox:%s\n", MSG_process_self_PID(), 
	// 			target_mailbox_name);
	// }

	double real_finish_time = MSG_get_clock();

	xbt_free(data);

	// Compute slow down of the task by dividing task_time by (real_finish_time - real_start_time)
	double actual_life_time = real_finish_time - real_start_time;

	//double slowdown = actual_life_time / need.expected_time;

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
	int ret = MSG_OK;
	while ((ret = MSG_task_send(fin_msg, fin_mailbox)) != MSG_OK)
		XBT_INFO("fail to send fin message\n");


	if (cluster_id == 0 &&  IS_ENABLE_GUEST_LOAD_BALANCE )
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
	

	sprintf(mailbox_id, "mailbox_%d_%d", cluster_id, mailbox_no);

	// We must start to listen to incoming messages asynchronously and destroy the message after getting.
	while (1)
	{
		msg_task_t r_msg = NULL;
		int ret = MSG_OK;
		while ((ret = MSG_task_receive(&r_msg, mailbox_id)) != MSG_OK)
		{
			if(ret != MSG_TIMEOUT )
				XBT_INFO("FAIL to receive on mailbox: %s\n", mailbox_id);
		}

		char* data = MSG_task_get_data(r_msg);
		if (data != NULL)
			if (!strcmp(data, "finish") )
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
				// Compute marginal cost
				marginal_cost =
							pow(number_of_vm, ((double)(vm_load + 1) / max_guest_job[cluster_id])) +
							pow(number_of_vm, (mem_usage + request->memory) / MAX_VM_MEMORY) +
							pow(number_of_vm, (net_usage + request->net) / MAX_VM_NET) -
							pow(number_of_vm, (double)(vm_load) / max_guest_job[cluster_id]) -
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
// static int policy_func_min_comm()
// {
// 	return 0;
// }

// static int policy_func_inhab_mig()
// {
// 	return 0;
// }

static int host_load_balance(int argc, char* argv[])
{
	if (argc < 3)
	{
		XBT_INFO("host load balancer: must be passed cluster_id assign policy\n");
		return -1;
	}

	//int cluster_id = atoi(argv[1]);
	host_assign_policy policy = atoi(argv[2]);

	// A loop that checks every K (default is 60) seconds, the load imbalance of hosts of cluster to make decision
	// on reassignment based on the given policy.
	// It will be terminated on receiving finish message perhaps!
	while(1)
	{
		MSG_process_sleep(60);

		switch (policy)
		{
			case INHAB_MIG:
				break;
			case MIN_COMM:
				break;
			default:
				return -2;
		};
	}

	return 0;
}

// Creates tasks and applies them to a machine.
// To be same for all executions, we must consider two separate clusters that the created tasks will submit in
// the order.
static int create_tasks(int argc, char* argv[])
{
	if (argc < 2)
	{
		XBT_INFO("Must pass therate of task arrival\n");
		return -1;
	}

	int number_of_processes = NUMBER_OF_PROCESS ;
	int number_of_vms =  NUMBER_OF_VMS ;
	unsigned time_seed = NUMBER_OF_PROCESS;
	double process_arrival_rate = atof(argv[1]);

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

	int cluster_id = 0 ;
	for (; cluster_id < NUMBER_OF_CLUSTERS ; ++cluster_id)
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
		arrival = ((double)rand()) / RAND_MAX;
		arrival = arrival == 1 ? arrival - 0.005 : arrival;
		double sleep_time = -log(1 - arrival) / process_arrival_rate;
		MSG_process_sleep(sleep_time);

		r = rand();
		m = rand();
		c = rand();
		rand_task = rand();
		target_mailbox = rand();

		compute_resource_need(rand_task, r, m, c, &need);

		for (cluster_id = 0; cluster_id < NUMBER_OF_CLUSTERS ; ++cluster_id)
		{

			if( IS_ENABLE_GUEST_LOAD_BALANCE )
			{
				// Only first cluster does load balance 
				if (cluster_id == 0 ) // here based on the type of task , load balancer decide about the home_vm to do the task.
				{
					rand();

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
					home_vm = rand() % number_of_vms;
					//XBT_INFO("cluster_id: %d , home vm: %d", cluster_id, home_vm);
				}
			} 
			else // load balance is disable
			{
				if(cluster_id == 0)
					home_vm = rand() % number_of_vms;
				else
					home_vm = home_vm; // previes value in cluster_id = 0;
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

			if (cluster_id == 0 &&  IS_ENABLE_GUEST_LOAD_BALANCE )
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

	int number_of_processes = NUMBER_OF_PROCESS; 
	int number_of_vms = NUMBER_OF_VMS ;

	char fin_mailbox[20];
	sprintf(fin_mailbox, "fin_mailbox");

	


	// We must start to listen to incoming messages asynchronously and destroy the message after getting.
	int i = 0;
	for (; i < NUMBER_OF_CLUSTERS * number_of_processes; ++i)
	{
		msg_task_t r_msg = NULL;
		int ret = MSG_OK;
		while ((ret = MSG_task_receive(&r_msg, fin_mailbox) != MSG_OK))
			XBT_INFO("faile to receive the fin from process\n");

		MSG_task_destroy(r_msg);
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

		if (cluster_id == 0 && IS_ENABLE_GUEST_LOAD_BALANCE )
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
	if( is_organizer_running )
	{
		MSG_process_kill(ORGANIZER_PROCESS);
		printf("number of migration is %d\n",migrate_counter );
		printf("########### END OF ORGANIZATION ###########\n");
	}
	printf("############### SIMULATAION_FINISHED ###################\n");
	return 0;
}

// Put this two functions in a process to be sync with other parts
// We assume that hosts are in a dynar based on their number and cluster declaration in cluster.xml
static void launch_master(unsigned no_vm, int no_process, int cluster_id)
{
	// create an empty array to save slowDown value of each VM.
	slow_down[cluster_id] = xbt_dynar_new(sizeof(SlowDown), NULL);
	// create an empty array to save data of each PM
	dataOfPMs[cluster_id] = xbt_dynar_new(sizeof(DATA_OF_PM),NULL);
	// Initalize a SlowDown object with 0 and add it to the SlowDown array.
	int i = 0;
	for (; i < no_process; ++i)
	{
		SlowDown slow_down_process;
		memset(&slow_down_process, 0, sizeof(slow_down_process));
		xbt_dynar_set(slow_down[cluster_id], i, &slow_down_process);
	}
	for(i = 0;i<NUMBER_OF_INVOLVED_HOST;i++)
	{
		DATA_OF_PM pmData;
		memset(&pmData, 0, sizeof(pmData));
		xbt_dynar_set(dataOfPMs[cluster_id], i, &pmData);
	}

	// create an empty array of VMs for each cluster.
	vm_list[cluster_id] = xbt_dynar_new(sizeof(msg_vm_t), NULL);

	// Number of VM per host
	int numberOfVM_perHost = no_vm / NUMBER_OF_INVOLVED_HOST;

	msg_host_t pm = NULL;

	// VM Parameters
	char vm_name[20];
	long ramsize = 1L * 1000 * 1000 * 1000; // 1Gbytes
	memory_max = ramsize / 1000;
	net_max = MAX_VM_NET / numberOfVM_perHost;
	cpu_max_flops = MSG_get_host_speed(*((msg_host_t*)xbt_dynar_get_ptr(MSG_hosts_as_dynar(), 0)));

	int j = 0;
	int host = (cluster_id) * NUMBER_OF_INVOLVED_HOST;
	int cuerrentCluster_host_counter = 0;
	for (i = 0; i < no_vm; ++i, ++j)
	{
		if (j >= numberOfVM_perHost)
		{
			++host;
			cuerrentCluster_host_counter++;
			j = 0;
		}

		pm = xbt_dynar_get_as(hosts_dynar, host, msg_host_t);

		sprintf(vm_name, "vm_%d_%d",cluster_id, i);

		s_vm_params_t params;
    	memset(&params, 0, sizeof(params));
    	params.ramsize = ramsize;
    	params.max_downtime = 1 ; // 1 second

		// create a VM object. A VM object is like a Host and it's name must be unique among all of the hosts.
		msg_vm_t vm = MSG_vm_create_core(pm, vm_name);
		MSG_host_set_params(vm, &params);

		// Add created VM to the list of VMs.
		xbt_dynar_set(vm_list[cluster_id], i, &vm);

		// save data of vm in it's pm
		DATA_OF_PM data;
		data = xbt_dynar_get_as(dataOfPMs[cluster_id],cuerrentCluster_host_counter,DATA_OF_PM);

		data.VMs[data.numberOfVMs] = i;
		data.numberOfVMs++;
		data.PM_name = MSG_host_get_name(pm);
		xbt_dynar_set_as(dataOfPMs[cluster_id],cuerrentCluster_host_counter,DATA_OF_PM,data);

		// TODO: check if need to do it with strings
		double* mem = (double*)malloc(sizeof(double));
		*mem = 0.0;
		MSG_host_set_property_value(vm, "mem", (char*)mem, free_double);

		double* net = (double*)malloc(sizeof(double));
		*net = 0.0;
		MSG_host_set_property_value(vm, "net", (char*)net, free_double);
		
		MSG_vm_start(vm);
	}

	if (cluster_id == 0  && IS_ENABLE_GUEST_LOAD_BALANCE )
	{
		int process_argc = 2;
		char process_name[40];
		char** process_argv = xbt_new(char*, 2);
		sprintf(process_name, "load_balancer_%d", cluster_id);
		process_argv[0] = xbt_new0(char, 40);
		sprintf(process_argv[0], "%s", process_name);
		process_argv[1] = xbt_new0(char, 40);
		sprintf(process_argv[1], "%d", cluster_id);
		msg_process_t load_balancer = MSG_process_create_with_arguments(process_name,
				guest_load_balance,
				NULL,
				xbt_dynar_get_as(hosts_dynar,
					++host, msg_host_t),
				process_argc,
				process_argv);
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
		mean_slowdown += slowdown;
	}

	mean_slowdown/=no_process;
	printf("cluster: %d slow down is : %f\n", cluster_id, mean_slowdown);
	// Use information before free
	xbt_dynar_remove_n_at(slow_down[cluster_id], no_process, 0);
	xbt_dynar_free(&slow_down[cluster_id]);


	xbt_dynar_remove_n_at(dataOfPMs[cluster_id],NUMBER_OF_INVOLVED_HOST,0);
	xbt_dynar_free(&dataOfPMs[cluster_id]);

	
}

// createing matrix to save organization data
void create_organization_matrix(int no_vm)
{
	int i;
	transmissionRate = (int**)malloc(sizeof(int*) * no_vm);
	for (i = 0; i < no_vm; i++)
		transmissionRate[i] = (int*)malloc(sizeof(int) * no_vm);

	transmissionLatency = (float**)malloc(sizeof(float*) * no_vm);
	for (i = 0; i < no_vm; i++)
		transmissionLatency[i] = (float*)malloc(sizeof(float) * no_vm);

	int j = 0;
	for( i = 0 ;i<no_vm;i++)
	{
		for(j = 0;j<no_vm;j++)
		{
			transmissionRate[i][j] = INITIAL_TRANMISSION_RATE;
			transmissionLatency[i][j] =  INITIAL_TRANMISSION_LATENCY;
		}
	}

}
// deleting matrix that save organization data
void delete_organization_matrix(int no_vm)
{	
	int i;
	for (i = 0; i < no_vm; i++)
	{
		free(transmissionRate[i]);
		free(transmissionLatency[i]);
	}
	free(transmissionRate);
	free(transmissionLatency);
}

static int organization_manager(int argc, char* argv[])
{	
	// Ya Sattar
	if (argc < 4)
	{
		XBT_INFO("Must pass:\n\
				organization period time\n\
				VM_DOWN_TIME_SERVICE_SAME_CLUSTER\n\
				VM_DOWN_TIME_SERVICE_OTHER_CLUSTER");
		return 0;
	}
	int SLEEP_TIME = atoi(argv[1]);
	if(SLEEP_TIME < 0 ) // run without organizer
	{
		is_organizer_running = 0;
		printf("########### RUN WITHOUT ORGANIZATION ############\n");
		return 0;
	}
	is_organizer_running = 1;
	VM_DOWN_TIME_SERVICE_SAME_CLUSTER = atof(argv[2]);
	//printf("VM_DOWN_TIME_SERVICE_SAME_CLUSTER=%f\n",VM_DOWN_TIME_SERVICE_SAME_CLUSTER);
	VM_DOWN_TIME_SERVICE_OTHER_CLUSTER = atof(argv[3]);
	//printf("VM_DOWN_TIME_SERVICE_OTHER_CLUSTER=%f\n",VM_DOWN_TIME_SERVICE_OTHER_CLUSTER);
	ORGANIZER_PROCESS = MSG_process_self();

	// organizer just work on cluseter_id = 0;
	int cluster_id = 0;

	int lastCandidateVMIndex = -1;
	// for test, migrate 1 vm from first host to the last host.
	printf("########### START OF ORGANIZATION ############\n");
	float H[NUMBER_OF_VMS];
	float nextH[NUMBER_OF_INVOLVED_HOST];

	int i = 0 ;
	int j = 0;

	// make a copy of organization matrix

	// make tmpMatrix
	int **tmpTransmissionRate = (int**)malloc(sizeof(int*) * NUMBER_OF_VMS);
	for (i = 0; i < NUMBER_OF_VMS; i++)
		tmpTransmissionRate[i] = (int*)malloc(sizeof(int) * NUMBER_OF_VMS);

	float** tmpTransmissionLatency = (float**)malloc(sizeof(float*) * NUMBER_OF_VMS);
	for (i = 0; i < NUMBER_OF_VMS; i++)
		tmpTransmissionLatency[i] = (float*)malloc(sizeof(float) * NUMBER_OF_VMS);

	// init tmpMatrixes
	for( i = 0;i<NUMBER_OF_VMS;i++)
		{
			for(j = 0;j<NUMBER_OF_VMS;j++)
			{
				tmpTransmissionRate[i][j] = INITIAL_TRANMISSION_RATE;
				tmpTransmissionLatency[i][j] =  INITIAL_TRANMISSION_LATENCY;
			}
		}


	for(; ; )
	{	
	 	MSG_process_sleep(SLEEP_TIME); // the sleep time is in simulation time!
		// copy original matrix to tmp
		for( i = 0;i<NUMBER_OF_VMS;i++)
		{
			for(j = 0;j<NUMBER_OF_VMS;j++)
			{
				tmpTransmissionRate[i][j] = (ALPHA * tmpTransmissionRate[i][j]) + ((1 - ALPHA ) * transmissionRate[i][j]);
				//tmpTransmissionRate[i][j] = ( transmissionRate[i][j]);

				tmpTransmissionLatency[i][j] =  (ALPHA * tmpTransmissionLatency[i][j]) + ((1 - ALPHA ) * transmissionLatency[i][j]);
				//tmpTransmissionLatency[i][j] =  ( transmissionLatency[i][j]);


				// clear original matrix to next period 
				transmissionRate[i][j] = INITIAL_TRANMISSION_RATE;
				transmissionLatency[i][j] = INITIAL_TRANMISSION_LATENCY;
			}
		}
	
		// calculate AvgLetancy
		// for(i = 0;i<NUMBER_OF_VMS;i++)
		// {
		// 	for(j = 0;j<NUMBER_OF_VMS;j++)
		// 	{
		// 		tmpTransmissionLatency[i][j] /= tmpTransmissionRate[i][j];
		// 	}
		// }
		

		// reseting H and nextH for current period
		for(i = 0;i<NUMBER_OF_VMS;i++)
			H[i] = 0.f;
		for(i = 0;i<NUMBER_OF_INVOLVED_HOST;i++)
			nextH[i] = 0.f;

		// calculating H for each vm
		for(i = 0;i< NUMBER_OF_VMS;i++)
		{
			for(j = 0;j<NUMBER_OF_VMS;j++)
			{ 
				H[i] += (tmpTransmissionRate[i][j] * tmpTransmissionLatency[i][j]) ;
				H[i] += (tmpTransmissionRate[j][i] * tmpTransmissionLatency[j][i]) ;
				// if( i == 14 && migrate_counter == 0 )
				// {
				// 	printf("H[%d]=%.3f with other VM[%d]\n",i,H[i],j);
				// }
			}
		}
		// find maximum H and name it as currentH
		float currentH  = 0.f;
		int candidateVM_index = 0;
		for(i = 0;i<NUMBER_OF_VMS;i++)
		{
			if( H[i] > currentH && i != lastCandidateVMIndex )
			{
				currentH = H[i];
				candidateVM_index = i;
			}
		}
			
		// calculating nextH by replacing currentH VM on all other PM.
			// find a VM on each of ther PMs as that PMs agent
			

		// findig a VM that run on the given PM(i) and name it as agent!
		

		// calculating nextH[i]  for each PM
		for(i = 0 ;i < NUMBER_OF_INVOLVED_HOST ;i++)
		{
			DATA_OF_PM data;
			data = xbt_dynar_get_as(dataOfPMs[cluster_id],i,DATA_OF_PM);
			for(j = 0 ;j< NUMBER_OF_VMS;j++)
			{
					int k = 0;
					int agent = data.VMs[k]; // we soppose the first vm on each PM as agent!
					float avg_host_to_j = 0.f , avg_j_to_host = 0.f;
					for( k = 0; k < data.numberOfVMs ; k++)
					{
						agent = data.VMs[k];
						avg_host_to_j += tmpTransmissionLatency[agent][j];
						avg_j_to_host += tmpTransmissionLatency[j][agent];
					}
					if( data.numberOfVMs > 0)
					{
						avg_host_to_j /= data.numberOfVMs;
						avg_j_to_host /= data.numberOfVMs;
					}
					else
					{
						avg_host_to_j = INITIAL_TRANMISSION_LATENCY;
						avg_j_to_host = INITIAL_TRANMISSION_LATENCY;
					}
					nextH[i] += (tmpTransmissionRate[candidateVM_index][j] * avg_host_to_j);
					nextH[i] += (tmpTransmissionRate[j][candidateVM_index] * avg_j_to_host);

			}
		}

		int homPM_index = -1;
		for( i = 0;i<NUMBER_OF_INVOLVED_HOST ;i++)
		{
			msg_vm_t vm = xbt_dynar_get_as(vm_list[cluster_id], candidateVM_index, msg_vm_t);
			// find if sender and reciever are in the same cluster?
			sscanf(MSG_host_get_name(MSG_vm_get_pm(vm)),"c-%d.me",&homPM_index);
			if( (homPM_index < 10 && i < 10) || (homPM_index > 10 &&  i > 10) )
				nextH[i] += VM_DOWN_TIME_SERVICE_SAME_CLUSTER;
			else
				nextH[i] += VM_DOWN_TIME_SERVICE_OTHER_CLUSTER;
		}

		// sort nextH from minimum to maximum
		int sortetIndexNextH[NUMBER_OF_INVOLVED_HOST];
		for (i = 0; i < NUMBER_OF_INVOLVED_HOST; i++)
		{
			sortetIndexNextH[i] = i;
		}


		// sort nextH in a new array based on nextH index
		int c, d;
		float swap = 0.f;
		for (c = 0; c < (NUMBER_OF_INVOLVED_HOST - 1); c++)
		{
			for (d = 0; d < NUMBER_OF_INVOLVED_HOST - c - 1; d++)
			{
				if (nextH[sortetIndexNextH[d]] > nextH[sortetIndexNextH[d+1]] ) /* For decreasing order use < */
				{
					swap = sortetIndexNextH[d];
					sortetIndexNextH[d] = sortetIndexNextH[d + 1];
					sortetIndexNextH[d + 1] = swap;
				}
			}
		}

		float VM_DOWN_TIME_SERVICE = 0.f;
	
		for( i = 0;i<NUMBER_OF_INVOLVED_HOST;i++)
		{
			DATA_OF_PM data;
			data = xbt_dynar_get_as(dataOfPMs[cluster_id],sortetIndexNextH[i],DATA_OF_PM);

			msg_vm_t vm = xbt_dynar_get_as(vm_list[cluster_id], candidateVM_index, msg_vm_t);

			// // find if sender and reciever are in the same cluster?
			// sscanf(MSG_host_get_name(MSG_vm_get_pm(vm)),"c-%d.me",&homPM_index);
			// if( (homPM_index < 10 && sortetIndexNextH[i] < 10) || (homPM_index > 10 && sortetIndexNextH[i] > 10) )
			// 	VM_DOWN_TIME_SERVICE = VM_DOWN_TIME_SERVICE_SAME_CLUSTER;
			// else
			// 	VM_DOWN_TIME_SERVICE = VM_DOWN_TIME_SERVICE_OTHER_CLUSTER;

			// if( i == 0 && currentH > 5.f)
			// {
			// 	printf("%d < %d && (%f + %f) < %f \n",data.numberOfVMs,PM_CAPACITY,nextH[sortetIndexNextH[i]], VM_DOWN_TIME_SERVICE, currentH );
			// }

			if( data.numberOfVMs < PM_CAPACITY && currentH - nextH[sortetIndexNextH[i]] > 5.5 )
			{
				
				// update lastCandidateVMIndex
				lastCandidateVMIndex = candidateVM_index;
				//printf("lastCondidVM:%d\n",lastCandidateVMIndex);

				//migrate candidateVM_index to targetPM_index;
				msg_host_t targetPM = xbt_dynar_get_as(hosts_dynar, sortetIndexNextH[i], msg_host_t);
				if(strcmp(MSG_host_get_name(MSG_vm_get_pm(vm)),MSG_host_get_name(targetPM)) == 0)
				{
					printf("###########WARNING_START###########\n");
					printf("migrate candidate vm_%d(%s) from PM %s to PM %s\n",candidateVM_index,MSG_host_get_name(vm),
						MSG_host_get_name(MSG_vm_get_pm(vm)),MSG_host_get_name(targetPM));

						printf("number Of VMs = %d < 4  and nextH:%.3f + D:%.3f < currnetH : %.3f\n",
							 data.numberOfVMs,nextH[sortetIndexNextH[i]], VM_DOWN_TIME_SERVICE, currentH);
					printf("###########WARNING_END##########\n");
				}
				else
				{
					homPM_index =0;
					for(j = 0;j< NUMBER_OF_INVOLVED_HOST;j++)
					{
						DATA_OF_PM data;
						data = xbt_dynar_get_as(dataOfPMs[cluster_id],j,DATA_OF_PM);

						if( strcmp(data.PM_name,MSG_host_get_name(MSG_vm_get_pm(vm)) ) == 0) // we find the hostPM in the dataOfPMs array
						{
							homPM_index = j;
							break;
						}
					}

					printf("%d) migrating %s from %s to %s cause nextH[%d]=%.3f < currentH=%.3f\n",migrate_counter+1,
						MSG_host_get_name(vm),MSG_host_get_name(MSG_vm_get_pm(vm)),MSG_host_get_name(targetPM),sortetIndexNextH[i],nextH[sortetIndexNextH[i]],currentH);

					// printf("migrating %s from PM[%d] to PM[%d] cause nextH[%d]=%.3f < currentH=%.3f\n",
					// 	MSG_host_get_name(vm),homPM_index,sortetIndexNextH[i],sortetIndexNextH[i],nextH[sortetIndexNextH[i]],currentH);

					// printf("number Of VMs = %d < 4  and nextH:%.3f + D:%.3f < currnetH : %.3f\n",
					// 		 data.numberOfVMs,nextH[sortetIndexNextH[i]], VM_DOWN_TIME_SERVICE, currentH);
				}

				migrate_vm(vm,candidateVM_index,targetPM,sortetIndexNextH[i]);
				migrate_counter++;
				// printf("number Of VMs = %d < 4  and nextH:%.3f + D:%.3f < currnetH : %.3f\n",
				// 	 data.numberOfVMs,nextH[sortetIndexNextH[i]], VM_DOWN_TIME_SERVICE, currentH);
				break;
			}
		}
		
	}
	// printf("number of migration is %d\n",migrate_counter );
	// printf("########### END OF ORGANIZATION ###########\n");
	return 0;
}

void migrate_vm(msg_vm_t vm,int VM_Index,msg_host_t targetPM,int targetPM_Index)
{
	//	- update structure and place of VM on PMs
	// remove the vm from the list of VMs of the host PM
	int i = 0;
	int cluster_id = 0;
	msg_host_t hostPM = MSG_vm_get_pm(vm);

	// finding hostPM in hostDynar
	for(i = 0;i< NUMBER_OF_INVOLVED_HOST;i++)
	{
		DATA_OF_PM data;
		data = xbt_dynar_get_as(dataOfPMs[cluster_id],i,DATA_OF_PM);

		if( strcmp(data.PM_name,MSG_host_get_name(hostPM) ) == 0) // we find the hostPM in the dataOfPMs array
		{
			// find vm_index in data.VMs
			int j = 0;
			for(j = 0;j<data.numberOfVMs;j++)
			{
				if(data.VMs[j] == VM_Index) // we find the vm in VMs list
				{
					RemoveItemFromArray(j,data.VMs,data.numberOfVMs); // RemoveItemFromArray(index,array,arraySize)
					data.numberOfVMs--;

					xbt_dynar_set_as(dataOfPMs[cluster_id],i,DATA_OF_PM,data);
					break;
				}
			}
			break;
		}
	}

	// add the vm to list of VMs of the targetPM
	DATA_OF_PM data;
	data = xbt_dynar_get_as(dataOfPMs[cluster_id],targetPM_Index,DATA_OF_PM);
	data.VMs[data.numberOfVMs] = VM_Index;
	data.numberOfVMs++;
	xbt_dynar_set_as(dataOfPMs[cluster_id],targetPM_Index,DATA_OF_PM,data);


	MSG_vm_migrate(vm,targetPM);
	send a comm_task with size VM memory size
	double ramsize = 1.f * 1000 * 1000 * 1000; // 1Gbytes
	msg_task_t comm_task = MSG_task_create("simulating_hot_migration_cost", 0,ramsize, NULL);
	
	char target_mailbox_name[40];
	sprintf(target_mailbox_name, "mailbox_0_%d",VM_Index);
	while (MSG_task_send(comm_task,target_mailbox_name) != MSG_OK)
	{
		printf("can not migrate VM %d to PM %d \n",VM_Index,targetPM_Index );
	}
	
}

int send_task( msg_task_t task, const char *mailBoxName,msg_vm_t vmSender, msg_vm_t vmReceiver)
{
	msg_error_t retValue = MSG_OK;
	double startTime = 0.f,endTime = 0.f;

	const char *senderPM_name,*receiverPM_name;
	senderPM_name = MSG_host_get_name(MSG_vm_get_pm(vmSender));
	receiverPM_name = MSG_host_get_name(MSG_vm_get_pm(vmReceiver));

	startTime = MSG_get_clock();

	// new way to make dummy delay
	if( strcmp(senderPM_name,receiverPM_name) == 0 )// PMs are same, So reduce task size as simulation of sending it to colocated VM.
	{
		double task_size = MSG_task_get_bytes_amount(task);
		//printf("reduce task size from %f to %f\n",task_size,task_size * (1.f/6) );
		MSG_task_set_bytes_amount(task,	(task_size * 0.01f) ); // we suppose 600 us is delay in one cluster, when wm are colocted its' about (50 + 50)us .
	}

	retValue =  MSG_task_send(task,mailBoxName);

	endTime = MSG_get_clock();

	if( retValue == MSG_OK) // gathering data
	{
		int senderID = -1,receiverID = -1,clusterID = -1;
		sscanf(MSG_host_get_name(vmSender),"vm_%d_%d",&clusterID,&senderID);
		sscanf(MSG_host_get_name(vmReceiver),"vm_%d_%d",&clusterID,&receiverID);
		if(clusterID == 0)
		{
			// transmissionLatency[senderID][receiverID] += (endTime - startTime);
			// transmissionRate[senderID][receiverID] += 1;

			float oldAvg = transmissionLatency[senderID][receiverID];
			int oldRate = transmissionRate[senderID][receiverID];

			// calculating new avarege for transmissionLatency
			transmissionLatency[senderID][receiverID] = ( (oldAvg * oldRate) + (endTime - startTime) )/(oldRate + 1);
			transmissionRate[senderID][receiverID] += 1;
		}
		
	}
	return retValue;
	

}
int main(int argc, char *argv[])
{
	msg_error_t res = MSG_OK;

	if (argc < 5)
	{
		XBT_CRITICAL("Usage: %s cluster.xml deployment.xml <number of VMs per cluster> <number of processes>\n", argv[0]);
		exit(1);
	}
	/* Argument checking */
	MSG_init(&argc, argv);
	MSG_create_environment(argv[1]); // create environment with platform description file : cluster.xml

	// registering functions in simaulator table.
	MSG_function_register("create_tasks", create_tasks);
	MSG_function_register("process_task", process_task);
	MSG_function_register("process_mailbox", process_mailbox);
	MSG_function_register("get_finalize", get_finalize);
	MSG_function_register("guest_load_balance", guest_load_balance);
	MSG_function_register("host_load_balance", host_load_balance);
	// register Organizatoin manager
	MSG_function_register("organization_manager", organization_manager);

	// get all of host that defined in cluster.xml in an array with the same index start by 0.
	hosts_dynar = MSG_hosts_as_dynar();

	NUMBER_OF_VMS =  atoi(argv[3]);
	NUMBER_OF_PROCESS = atoi(argv[4]);

	
	NUMBER_OF_INVOLVED_HOST = (xbt_dynar_length(hosts_dynar) - 1) / NUMBER_OF_CLUSTERS ;
	
	ITERATION_PER_PROCESS = NUMBER_OF_VMS * ITERATION_PER_PROCESS_RATIO; 
	COMP_TASK_SIZE = COMP_PER_COMM * ITERATION_PER_PROCESS;
	COMM_TASK_SIZE = ITERATION_PER_PROCESS - COMP_PER_COMM; // to avoid infinite loop, not using (1-COMP_PER_COMM) * ITERATION_PER_PROCESS)

	//printf("NUMBER_OF_INVOLVED_HOST=%d\n",NUMBER_OF_INVOLVED_HOST);
	//NUMBER_OF_INVOLVED_HOST = (xbt_dynar_length(hosts_dynar) - 1); // minous one is for 1 master host that create and manage cluster (c-52).

	int i = 0;
	for (; i < NUMBER_OF_CLUSTERS; ++i)
	{
		vm_list[i] = NULL;
		max_guest_job[i] = 1; // to calculate load and marginalc cost-> I don't need it!
		launch_master(NUMBER_OF_VMS, NUMBER_OF_PROCESS, i);
	}
	create_organization_matrix(NUMBER_OF_VMS);

	// call regesterd function that described in deployment file with the parameter.
	MSG_launch_application(argv[2]); // argv[2]: deployment.xml

	// Main start-point of simulation
	res = MSG_main();

	for (i = 0; i < NUMBER_OF_CLUSTERS; ++i)
	{
		destroy_master(NUMBER_OF_VMS, NUMBER_OF_PROCESS, i);
		vm_list[i] = NULL;
	}
	delete_organization_matrix(NUMBER_OF_VMS);

	if (res == MSG_OK)
		printf("Simulation Finished successfully :)\n");
	else
		printf("Simulation Failed :(\n");
	printf("Simulation Time: %lf\n", MSG_get_clock());

	if (res == MSG_OK)
		return 0;
	else
		return 1;
}
