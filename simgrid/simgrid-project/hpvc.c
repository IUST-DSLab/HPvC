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

#define PIVOT (RAND_MAX / 2)
#define NUMBER_OF_PROCESSES 20
#define COMPUTING_NEED_FACTOR 1e+8
#define COMM_NEED_FACTOR 16e+6

typedef struct __attribute__((__packed__)) ProcessResourceConsumption
{
	long double cpu;
	long double memory;
	long double net;
} process_resource_consumption;

//xbt_mutex_t finish_guard;
static int finish = 0;

static xbt_dynar_t vm_list;

static xbt_dynar_t process_res_consumed;

int controller(int argc, char* argv[])
{
	if (argc < 2)
		return -1;
	MSG_process_sleep(atoi(argv[1]));
	XBT_INFO("Controller: Now it is time to finish\n");
	finish = 1;		// Must control this parameter no to have some sending processes without any receivers
	XBT_INFO("Controller: finish has been set\n");
	return 0;
}

// We are no longer need to this function. But we make sure this will be reminded unchanged until it becomes useless.
int process_job(int argc, char* argv[])
{
	if (argc < 2)
	{
		return -1;
	}
	int mailbox_id = atoi(argv[1]);
	int reciever_mailbox_id = 0;
	unsigned time_seed = (unsigned)MSG_get_clock();
	srand(time_seed);
	msg_process_t me = MSG_process_self();
	int me_pid = MSG_process_self_PID();
	msg_comm_t irecv = NULL;
	msg_comm_t isend = NULL;
	char mailbox[20];
	sprintf(mailbox, "mailbox_%d", mailbox_id);
	msg_task_t r_msg = NULL;
	msg_task_t s_msg = NULL;
	int has_msg = 0;
	int sent_msg = 0;
	msg_task_t my_task;
	unsigned long my_task_count = 0;
	char my_task_name[40];

	//XBT_INFO("Begin to loop %d\n", me_pid);
	while(!finish)
	{
		// This part of code simulate execution of some cpu task with a random flops need
		sprintf(my_task_name, "PID_%d_task_%lu", me_pid, my_task_count);
		//XBT_INFO("Executing new task PID_%d task %s\n", me_pid, my_task_name);
		my_task = MSG_task_create(my_task_name, ((double)(rand() + 1) / RAND_MAX) * COMPUTING_NEED_FACTOR, 0, NULL);
		MSG_task_execute(my_task);
		MSG_task_destroy(my_task);
		my_task = NULL;
		++my_task_count;
		MSG_process_sleep(1);

		if (rand() <= PIVOT)
		{
			if (!has_msg)
			{
				irecv = MSG_task_irecv(&r_msg, mailbox);
				//XBT_INFO("Register for new message PID_%d\n", me_pid);
				MSG_process_sleep(1);
				has_msg = 1;
			}
			if (irecv == NULL)
				continue;
			else if (MSG_comm_test(irecv) != 0)
			{
				msg_error_t status = MSG_comm_get_status(irecv);
				if( status == MSG_OK)
				{
					//XBT_INFO("Register for new message PID_%d\n", me_pid);
					//MSG_task_execute(r_msg);
					MSG_task_destroy(r_msg);
					r_msg = NULL;
					MSG_comm_destroy(irecv);
					irecv = NULL;
					has_msg = 0;
				}
				else
				{
					printf("receive field because of %d\n", status);
					if (irecv != NULL)
					{
						MSG_comm_destroy(irecv);
						irecv = NULL;
					}
					if (r_msg !=NULL)
					{
						MSG_task_destroy(r_msg);
						r_msg = NULL;
					}
					has_msg = 0;
				}
			}
			else
				MSG_process_sleep(1);
		}
		else		// rand() > PIVOT
		{
			if (!sent_msg)
			{
				char remote_mailbox[20];
				char s_msg_name[20];
				do
				{				
					reciever_mailbox_id = rand() % NUMBER_OF_PROCESSES;
				} while(reciever_mailbox_id == mailbox_id);
				sprintf(remote_mailbox, "mailbox_%d", reciever_mailbox_id);
				sprintf(s_msg_name, "msg_%d", mailbox_id);
				s_msg = MSG_task_create(s_msg_name, 0, ((double)(rand() + 1) / RAND_MAX) * COMM_NEED_FACTOR, NULL);
				//isend = MSG_task_isend(s_msg, remote_mailbox);
				MSG_task_dsend(s_msg, remote_mailbox, NULL);
				MSG_process_sleep(1);
				sent_msg = 1;
			}
			sent_msg = 0;
			//MSG_process_sleep(1);
		//	if (isend == NULL)
		//		continue;
		//	else if (MSG_comm_test(isend) != 0)
		//	{
		//		msg_error_t status = MSG_comm_get_status(isend);
		//		if( status == MSG_OK)
		//		{
		//			if( isend != NULL)
		//			{
		//				//XBT_INFO("Message sent from PID_%d\n", me_pid);
		//				MSG_comm_destroy(isend);
		//				isend = NULL;
		//			//	MSG_task_destroy(s_msg);
		//			//	s_msg = NULL;
		//				sent_msg = 0;
		//			}
		//		}
		//		else 
		//		{
		//			printf("send feild because of send error %d\n",status);
		//			if( isend != NULL)
		//			{
		//				MSG_comm_destroy(isend);
		//				isend = NULL;
		//			}
		//			sent_msg = 0;
		//		}
		//	
		//	}
		//	else
		//		MSG_process_sleep(1);
		}
	}
	XBT_INFO("PID_%d is going to be off\n", me_pid);
	return 0;
}

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

	if (((double)(rand_task) / RAND_MAX) < 0.95)
	{
		r = r == 0 ? 10 : r;
		cpu_need = (RAND_MAX / (double)(r)) * 1e9;		// Based on description of opportunity cost paper
																// MAX_FLOPS = 1e9
		m = m == 0 ? 10 : m;
		mem_need = (RAND_MAX / (double)(m)) * 1e9;		// MAX_MEM 0f each vm is 1e9

		msg_size = ((double)(c) / RAND_MAX) * cpu_need * 16e6;		// The msg_size based on cpu_need
																			// 16e6 is bandwidth in byte/sec
	}
	else
	{
		r = r == 0 ? 10 : r;
		cpu_need = (RAND_MAX * 10 / (double)(r)) * 1e9;		// Based on description of opportunity cost paper
																// MAX_FLOPS = 1e9
		m = m == 0 ? 10 : m;
		mem_need = (RAND_MAX / (double)(m)) * 1e9;		// MAX_MEM 0f each vm is 1e9

		msg_size = ((double)(c) / RAND_MAX) * cpu_need * 16e5;		// The msg_size based on cpu_need
																			// 16e6 is bandwidth in byte/sec
	}


	char exec_name[20];
	char msg_name[20];
	sprintf(exec_name, "task_%d", MSG_process_self_PID());
	sprintf(msg_name, "msg_%d", MSG_process_self_PID());
	char target_mailbox_name[40];
	sprintf(target_mailbox_name, "mailbox_%d", target_mailbox);
	double real_start_time = MSG_get_clock();

	// Do the task

	uint8_t* data = xbt_new0(uint8_t, (uint64_t)mem_need);

	msg_task_t executive_task = MSG_task_create(exec_name, cpu_need, 0, data);
	msg_task_t comm_task = MSG_task_create(msg_name, 0, msg_size, "");

	msg_error_t error = MSG_task_execute(executive_task);
	if (error != MSG_OK)
		XBT_INFO("Failed to execute task! %s\n", exec_name);

	// TODO: Check if this time is not contribute to the real time
	MSG_task_dsend(comm_task, target_mailbox_name, NULL);		

	double real_finish_time = MSG_get_clock();

	// Get task timing factors
	double cpu_time = RAND_MAX / (double)(r);
	double expected_time = cpu_time * (1 + (double)(c) / RAND_MAX);

	// Compute slow down of the task by dividing task_time by (real_finish_time - real_start_time)
	double actual_life_time = real_finish_time - real_start_time;

	double slowdown = actual_life_time / expected_time;

	XBT_INFO("PID_%d is going to be off with slowdown about: %f\n", MSG_process_self_PID(), slowdown);

	char fin_name[40];
	char fin_mailbox[20];
	sprintf(fin_name, "fin_%d", MSG_process_self_PID());
	sprintf(fin_mailbox, "fin_mailbox_%d", cluster_id);
	msg_task_t fin_msg = MSG_task_create(fin_name, 0, 1, NULL);
	MSG_task_dsend(fin_msg, fin_mailbox, NULL);

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
	msg_comm_t irecv = NULL;
	msg_task_t r_msg = NULL;

	sprintf(mailbox_id, "mailbox_%d_%d", cluster_id, mailbox_no);

	// We must start to listen to incoming messages asynchronously and destroy the message after getting.
	while (1)
	{
		irecv = MSG_task_irecv(&r_msg, mailbox_id);
		int ret = MSG_comm_wait(irecv, -1); // see the doc

		// it will see a goodbye meesage for termination
		if (!ret)
		{
			unsigned int data_size = MSG_task_get_data_size(r_msg);
			char* data = MSG_task_get_data(r_msg);
			if (!strncpy(data, "finish", 6))
			{
				MSG_comm_destroy(irecv);
				MSG_task_destroy(r_msg);
				break;
			}
		}
		MSG_comm_destroy(irecv);
		MSG_task_destroy(r_msg);
	}

	return 0;
}

// Creates tasks and applies them to a machine.
// To be same for all executions, we must consider two separate clusters that the created tasks will submit in
// the order.
static int create_tasks(int argc, char* argv[])
{
	if (argc < 6)
	{
		XBT_INFO("Must pass the number of process to be simulated,\
				the seed of random numbers, number of VMs and rate of task arrival\n");
		return -1;
	}

	int number_of_processes = atoi(argv[1]);
	unsigned time_seed = atoi(argv[2]);
	int number_of_vms = atoi(argv[3]);
	double process_arrival_rate = atof(argv[4]);
	int cluster_id = atoi(argv[5]);
	srand(time_seed);

	double arrival = 0;
	unsigned int r = 0; // the factor we use in jobs execution time
	unsigned int m = 0;
	unsigned int c = 0;
	unsigned int rand_task = 0;
	unsigned int home_vm = 0;
	unsigned int target_mailbox = 0;
	msg_process_t process = NULL;
	char process_name[40];
	char process_argv[200];
	int process_argc = 7;
	msg_vm_t host = NULL;
	
	// Loops until creates all processes. We think that number of processes must be larger than 10000.
	// 5% of tasks must be of 20/r CPU and 95% must be 2/r.
	// The communication time is c% of CPU time.
	int i = 0;
	for (; i < number_of_processes; ++i)
	{
		arrival = rand() / RAND_MAX;
		arrival = arrival == 1 ? arrival - 0.001 : arrival;
		double sleep_time = -log(1 - arrival) / process_arrival_rate;
		MSG_process_sleep(sleep_time);

		r = rand();
		m = rand();
		c = rand();
		rand_task = rand();
		home_vm = rand();
		target_mailbox = rand();

		sprintf(process_name, "process_%d", i);
		sprintf(process_argv, "%s %u %u %u %u %u %u %d", process_name, r, m, c,
				rand_task, home_vm % number_of_vms, target_mailbox % number_of_vms, cluster_id);
		
		// Find a host based on the random number from vm's list
		xbt_dynar_get_cpy(vm_list, home_vm % number_of_vms, host);

		process = MSG_process_create_with_arguments( process_name
												   , process_task
												   , NULL
												   , host
												   , process_argc
												   , (char**)&process_argv);

	}

	finish = 1;

	return 0;
}

// It waits for incoming messages from all processes that says they have already been finished
// Afterwards, it sends all vm listeners a message to finish listening
static int get_finalize(int argc, char* argv[])
{
	if (argc < 4)
	{
		XBT_INFO("get finalize must be passed cluster number, number of processes and vms\n");
		return -1;
	}

	int cluster_id = atoi(argv[1]);
	int number_of_processes = atoi(argv[2]);
	int number_of_vms = atoi(argv[3]);

	char fin_mailbox[20];
	sprintf(fin_mailbox, "fin_mailbox_%d", cluster_id);

	msg_comm_t irecv = NULL;
	msg_task_t r_msg = NULL;


	// We must start to listen to incoming messages asynchronously and destroy the message after getting.
	int i = 0;
	for (; i < number_of_processes; ++i)
	{
		irecv = MSG_task_irecv(&r_msg, fin_mailbox);
		int ret = MSG_comm_wait(irecv, -1); // see the doc
		MSG_comm_destroy(irecv);
		MSG_task_destroy(r_msg);
	}

	// We are sure about finalization of all processes
	MSG_process_sleep(10);

	// Send finish message to all vm listener
	char vm_mailbox[40];
	char fin_name[40];
	msg_task_t fin_msg = NULL;
	for (i = 0; i < number_of_vms; ++i)
	{
		sprintf(vm_mailbox, "mailbox_%d_%d", cluster_id, i);
		sprintf(fin_name, "finish_%d_%d", cluster_id, i);
		fin_msg = MSG_task_create(fin_name, 0, 6, "finish");
		MSG_task_dsend(fin_msg, vm_mailbox, NULL);
	}

	return 0;
}

//put this two functions in a process to be sync with other parts
static void launch_master(unsigned no_vm, unsigned no_process)
{
	vm_list = xbt_dynar_new(sizeof(msg_vm_t), NULL);
	int host = 0;
	xbt_dynar_t hosts_dynar = MSG_hosts_as_dynar();
	int max_host= no_vm / xbt_dynar_length(hosts_dynar);
	msg_host_t pm;
	char vm_name[20];
	int i = 0;
	for (i = 0; i < no_vm; ++i)
	{
		host = i / 4;
		XBT_INFO("vm-%d is going to be created\n", i);
		pm = xbt_dynar_get_as(hosts_dynar, host, msg_host_t);
		sprintf(vm_name, "vm-%d", i);
		s_ws_params_t params;
		memset(&params, 0, sizeof(params));
		params.ramsize = 1L * 1000 * 1000 * 1000;
		msg_vm_t vm = MSG_vm_create_core(pm, vm_name);
		MSG_host_set_params(vm, &params);
		xbt_dynar_set(vm_list, i, &vm);
		MSG_vm_start(vm);
		XBT_INFO("vm-%d is started\n", i);
	}
	process_res_consumed = xbt_dynar_new(sizeof(process_resource_consumption), NULL); 
	for (i = 0; i < no_process; ++i)
	{
		process_resource_consumption prc;
		memset(&prc, 0, sizeof(process_resource_consumption));
		xbt_dynar_set(process_res_consumed, i, &prc);
	}
}

static void destroy_master(unsigned no_vm, unsigned no_process)
{
	msg_vm_t vm;
	process_resource_consumption prc;
	int i = 0;
	while (!xbt_dynar_is_empty(process_res_consumed))
	{
		++i;
		xbt_dynar_remove_at(process_res_consumed, 0, &prc);
	}
	xbt_dynar_free(&process_res_consumed);

	i = 0;
	while (!xbt_dynar_is_empty(vm_list))
	{
		++i;
		XBT_INFO("vm-%d is going to be destroyed\n", i);
		xbt_dynar_remove_at(vm_list, 0, &vm);
		if (MSG_vm_is_running(vm))
			MSG_vm_shutdown(vm);
		if (MSG_vm_is_suspended(vm))
		{
			MSG_vm_resume(vm);
			MSG_vm_shutdown(vm);
		}
		MSG_vm_destroy(vm);
		XBT_INFO("vm-%d is destroyed\n", i);
	}
	xbt_dynar_free(&vm_list);
}

int main(int argc, char *argv[])
{
	msg_error_t res = MSG_OK;

	if (argc < 4)
	{
		XBT_CRITICAL("Usage: %s cluster.xml deployment.xml <number of VMs> \n", argv[0]);
		exit(1);
	}
	/* Argument checking */
	MSG_init(&argc, argv);
	MSG_create_environment(argv[1]);
    MSG_function_register("process_job", process_job);
    MSG_function_register("controller", controller);
	MSG_function_register("create_tasks", create_tasks);
	MSG_function_register("process_task", process_task);
	MSG_function_register("process_mailbox", process_mailbox);
	MSG_function_register("get_finalize", get_finalize);

	launch_master(atoi(argv[3]), NUMBER_OF_PROCESSES);
	MSG_launch_application(argv[2]);

	res = MSG_main();

	destroy_master(atoi(argv[3]), NUMBER_OF_PROCESSES);

	XBT_INFO("Simulation time %g\n", MSG_get_clock());
	if (res == MSG_OK)
		return 0;
	else
		return 1;

}
