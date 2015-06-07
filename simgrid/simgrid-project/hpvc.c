/**
 * Copyright (c) 2015-2016. The DSLab HPC Team
 * All rights reserved. 
 * Developers: Sina Mahmoodi, Mostafa Zamani
 * This is file we used to develop the HPvC simulator
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
int process_task(int argc, char* argv[])
{
	// Create the task with respect to parameters

	double real_start_time = MSG_get_clock();

	// Do the task

	double real_finish_time = MSG_get_clock();

	// Get task timing factors
	
	// Compute slow down of the task by dividing task_time by (real_finish_time - real_start_time)
	

	return 0;
}

// Creates tasks and applies them to a machine.
// To be same for all executions, we must consider two separate clusters that the created tasks will submit in
// the order.
int create_tasks(int argc, char* argv[])
{
	if (argc < 4)
	{
		XBT_INFO("Must pass the number of process to be simulated, the seed of random numbers and rate of task arrival\n");
		return -1;
	}

	int number_of_processes = atoi(argv[1]);
	unsigned time_seed = atoi(argv[2]);
	double r = 0; // the factor we use in jobs execution time
	double m = 0;
	double c = 0;
	int target_vm = 0;

	// Loops until creates all processes. We think that number of processes must be larger than 10000.
	// 5% of tasks must be of 20/r CPU and 95% must be 2/r.
	// The communication time is c% of CPU time.
	int i = 0;
	for (; i < number_of_processes; ++i)
	{

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
