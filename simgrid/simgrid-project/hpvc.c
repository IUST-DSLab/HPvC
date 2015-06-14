/**
 * Copyright (c) 2015-2016. The DSLab HPC Team
 * All rights reserved. 
 * Developers: Sina Mahmoodi, Mostafa Zamani
 * This is file we used to develop the HPvC simulator
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "msg/msg.h"            /* core library */
#include "xbt/sysdep.h"         /* calloc */
#include "xbt/synchro_core.h"

#include "xbt/log.h"
#include "xbt/asserts.h"
XBT_LOG_NEW_DEFAULT_CATEGORY(msg_test, "Messages specific for this msg example");

#define NUMBER_OF_CLUSTERS 2

typedef struct __attribute__((__packed__)) ProcessResourceConsumption
{
	long double cpu;
	long double memory;
	long double net;
} process_resource_consumption;

static xbt_dynar_t vm_list[NUMBER_OF_CLUSTERS];

static xbt_dynar_t hosts_dynar;
static int number_of_involved_host; 
static int total_cluster_host;

// Create process and a task with the passing parameters of CPU, Memory and communication.
// Communication will be done with another VM that is provided by create_task process.
static int process_task(int argc, char* argv[])
{
	// Create the task with respect to parameters
	if (argc < 8)
	{
		XBT_INFO("Each process must be passed 6 arguments\n");
		return -1;
	}

	unsigned int r = atoi(argv[1]);
	unsigned int m = atoi(argv[2]);
	unsigned int c = atoi(argv[3]);
	unsigned int rand_task = atoi(argv[4]);
	unsigned int home_vm = atoi(argv[5]);
	unsigned int target_mailbox = atoi(argv[6]);
	int cluster_id = atoi(argv[7]);

	double cpu_need;
	double mem_need;
	double msg_size;

	double cpu_time;
	double expected_time;

	if (((double)(rand_task) / RAND_MAX) < 0.95)
	{
		r = r <= 10 ? 10 : r;
		cpu_need = (RAND_MAX / (double)(r)) * 1e9;		// Based on description of opportunity cost paper
																// MAX_FLOPS = 1e9
		m = m <= 10 ? 10 : m;
		mem_need = (RAND_MAX / (double)(m)) * 1e5;		// MAX_MEM 0f each vm is 1e9

		c = c <= 10? 10 : c;
		msg_size = (RAND_MAX / (double)(c)) * 3.2e6;		// The msg_size based on cpu_need
																			// 16e6 is bandwidth in byte/sec
		cpu_time = RAND_MAX / (double)(r);
		expected_time = cpu_time + msg_size / 1.25e8;
	}
	else
	{
		r = r <= 10 ? 10 : r;
		cpu_need = (RAND_MAX  / (double)(r)) * 1e10;		// Based on description of opportunity cost paper
																// MAX_FLOPS = 1e9
		m = m <= 10 ? 10 : m;
		mem_need = (RAND_MAX / (double)(m)) * 1e5;		// MAX_MEM 0f each vm is 1e9

		c = c <= 10? 10 : c;
		msg_size = ((double)(c) / (double)(r)) * 3.2e6;		// The msg_size based on cpu_need
																	// 16e6 is bandwidth in byte/sec
		cpu_time = 10 * (RAND_MAX / (double)(r));
		expected_time = cpu_time  + msg_size / 1.25e8;
	}

	XBT_INFO("process with cpu:%f, memory: %f, net: %f on cluster: %d, vm: %d, pid: %d \
			cpu_time: %f, expected: %f\n",
			cpu_need, mem_need, msg_size, cluster_id, home_vm, MSG_process_self_PID(), cpu_time,
			expected_time);

	char exec_name[20];
	char msg_name[20];
	sprintf(exec_name, "task_%d", MSG_process_self_PID());
	sprintf(msg_name, "msg_%d", MSG_process_self_PID());
	char target_mailbox_name[40];
	sprintf(target_mailbox_name, "mailbox_%d_%d", cluster_id, target_mailbox);
	uint8_t* data = xbt_new0(uint8_t, (uint64_t)mem_need);
	msg_task_t comm_task = MSG_task_create(msg_name, 0, msg_size, NULL);

	double real_start_time = MSG_get_clock();

	// Do the task

	msg_task_t executive_task = MSG_task_create(exec_name, cpu_need, 0, data);

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

	double slowdown = actual_life_time / expected_time;

	XBT_INFO("PID_%d on cluster %d is going to be off with slowdown about: %f and actual time: %f, expected: %f\n",
			MSG_process_self_PID(), cluster_id, slowdown, actual_life_time, expected_time);

	char fin_name[40];
	char fin_mailbox[20];
	sprintf(fin_name, "fin_%d", MSG_process_self_PID());
	sprintf(fin_mailbox, "fin_mailbox");
	msg_task_t fin_msg = MSG_task_create(fin_name, 0, 1, NULL);
	ret = MSG_OK;
	while ((ret = MSG_task_send(fin_msg, fin_mailbox)) != MSG_OK)
		XBT_INFO("fail to send fin message\n");

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
	int process_argc = 8;
	msg_vm_t host = NULL;
	
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
		home_vm = rand();
		target_mailbox = rand();

		int cluster_id = 0;
		for (; cluster_id < NUMBER_OF_CLUSTERS; ++cluster_id)
		{
			char process_name[40];
			char** process_argv = xbt_new(char*, 8);
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
			
			// Find a host based on the random number from vm's list

			host = xbt_dynar_get_as(vm_list[cluster_id], home_vm % number_of_vms, msg_vm_t);

			process = MSG_process_create_with_arguments( process_name
													   , process_task
													   , NULL
													   , host
													   , process_argc
													   , process_argv);
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

	msg_comm_t irecv = NULL;
	msg_task_t r_msg = NULL;


	// We must start to listen to incoming messages asynchronously and destroy the message after getting.
	int i = 0;
	for (; i < NUMBER_OF_CLUSTERS * number_of_processes; ++i)
	{
		int ret = MSG_OK;
		while ((ret = MSG_task_receive(&r_msg, fin_mailbox) != MSG_OK))
			XBT_INFO("file to receive the fin from process\n");

		MSG_comm_destroy(irecv);
		MSG_task_destroy(r_msg);
		irecv = NULL;
		r_msg = NULL;
	}

	// We are sure about finalization of all processes
	MSG_process_sleep(10);

	// Send finish message to all vm listener
	char vm_mailbox[40];
	char fin_name[40];
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
			{
				XBT_INFO("FAILURE in sending fin to VMs\n");
			}
		}
	}

	MSG_process_sleep(10);
	return 0;
}

// Put this two functions in a process to be sync with other parts
// We assume that hosts are in a dynar based on their number and cluster declaration in cluster.xml
static void launch_master(unsigned no_vm, int cluster_id)
{
	vm_list[cluster_id] = xbt_dynar_new(sizeof(msg_vm_t), NULL);

	// Number of VM per host
	int vm_to_host = (NUMBER_OF_CLUSTERS * no_vm) / xbt_dynar_length(hosts_dynar);

	// The last is the one who dose management for this step
	int host = (cluster_id) * total_cluster_host;
	int max_host= host + no_vm / number_of_involved_host;

	msg_host_t pm = NULL;

	char vm_name[20];

	int i = 0;
	int j = 1;
	for (i = 0; i < no_vm; ++i, ++j)
	{
		if (j / number_of_involved_host)
		{
			++host;
			j = 1;
		}

		pm = xbt_dynar_get_as(hosts_dynar, host, msg_host_t);

		sprintf(vm_name, "vm_%d_%d", cluster_id, i);

		s_ws_params_t params;
		memset(&params, 0, sizeof(params));
		params.ramsize = 1L * 1000 * 1000 * 1000;

		msg_vm_t vm = MSG_vm_create_core(pm, vm_name);
		MSG_host_set_params(vm, &params);

		xbt_dynar_set(vm_list[cluster_id], i, &vm);

		MSG_vm_start(vm);
	}
}

static void destroy_master(unsigned no_vm, int cluster_id)
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
}

int main(int argc, char *argv[])
{
	msg_error_t res = MSG_OK;

	if (argc < 4)
	{
		XBT_CRITICAL("Usage: %s cluster.xml deployment.xml <number of VMs per cluster>\n", argv[0]);
		exit(1);
	}
	/* Argument checking */
	MSG_init(&argc, argv);
	MSG_create_environment(argv[1]);
	MSG_function_register("create_tasks", create_tasks);
	MSG_function_register("process_task", process_task);
	MSG_function_register("process_mailbox", process_mailbox);
	MSG_function_register("get_finalize", get_finalize);

	hosts_dynar = MSG_hosts_as_dynar();
	number_of_involved_host = (xbt_dynar_length(hosts_dynar) - 1) / NUMBER_OF_CLUSTERS - 1;
	total_cluster_host = (xbt_dynar_length(hosts_dynar) - 1) / NUMBER_OF_CLUSTERS;

	int i = 0;
	for (; i < NUMBER_OF_CLUSTERS; ++i)
	{
		vm_list[i] = NULL;
		launch_master(atoi(argv[3]), i);
	}

	MSG_launch_application(argv[2]);

	// Main start-point of simulation
	res = MSG_main();

	for (i = 0; i < NUMBER_OF_CLUSTERS; ++i)
	{
		destroy_master(atoi(argv[3]), i);
		vm_list[i] = NULL;
	}

	XBT_INFO("Simulation time %g\n", MSG_get_clock());

	if (res == MSG_OK)
		return 0;
	else
		return 1;
}
