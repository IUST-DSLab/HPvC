#ifndef __UTILITY_H__
#define __UTILITY_H__

#include "structures.h"

void RemoveItemFromArray(int index,int* array,int arraySize )
{
	int i = 0;
	// index must be little than arraySize
	if (index >= arraySize)
		return;
	for (i = index; i < arraySize - 1; i++)
		array[i] = array[i + 1];
}

// Get size of a task and calculate it delay when send from colocated VM
double GetTimeout(double task_size,double MAXIMUM_TASK_SIZE,int NUMBER_OF_VM_PER_HOST)
{
	double host_to_hot_delay = 0.0;
	// find approximate time when sendig this message form a host to another host
	int normalize = (task_size / MAXIMUM_TASK_SIZE) * 10; 

	host_to_hot_delay = 0;
	switch(normalize)
	{
		case 0:
			host_to_hot_delay = 0.109868;  // delay for 1 vm on each pm
			break;							
		case 1:
			host_to_hot_delay = 0.211930;
			break;
		case 2:
			host_to_hot_delay = 0.313992;
			break;
		case 3:
			host_to_hot_delay = 0.416053;
			break;
		case 4:
			host_to_hot_delay = 0.518115;
			break;
		case 5:
			host_to_hot_delay = 0.620177;
			break;
		case 6:
			host_to_hot_delay = 0.722239;
			break;
		case 7:
			host_to_hot_delay = 0.824301;
			break;
		case 8:
			host_to_hot_delay = 0.926363;
			break;
		case 9:
			host_to_hot_delay = 1.028425;
			break;
	}
	host_to_hot_delay /= NUMBER_OF_VM_PER_HOST;
	
	double collocateVM_reduce_delay_ratio = 0.5; // should be changed!
	double TIMEOUT = host_to_hot_delay  * collocateVM_reduce_delay_ratio;
	return TIMEOUT;
}
void PrintDataOfPM(DATA_OF_PM data)
{
	printf("#####DATA_OF_PM#####\n");
	printf("PM Name:%s\n",data.PM_name);
	printf("numberOfVms = %d\n",data.numberOfVMs );
	int i = 0;
	for(i = 0;i<data.numberOfVMs;i++)
	{
		printf("vm[%d]=%d\t", i,data.VMs[i]);
	}
	printf("\n#####DATA_OF_PM#####\n");
}
int GetNextTargetMailBox(int mailBoxID,int ProcessSequenceNumber,int nu_vm)
{
	return (mailBoxID + ProcessSequenceNumber)%nu_vm;
}
////////////////////////////////////////////////////////////////////////////////
/* testing send a message form vm to located vm and other vm
int q;
	for( q = 1;q<=10;q+=1)
	{
		msg_task_t comm_task = MSG_task_create("salam", 100.f, 0.5 * net_max, NULL);
		double start_time = MSG_get_clock();

		int ret = MSG_OK;		
		msg_vm_t vmSender,vmReceiver;
		vmSender = xbt_dynar_get_as(vm_list[0],0, msg_vm_t);
		vmReceiver = xbt_dynar_get_as(vm_list[0],q, msg_vm_t);

		char target_mailbox_name[40];
		sprintf(target_mailbox_name, "mailbox_0_%d", q);
		ret = send_task(comm_task, target_mailbox_name,vmSender,vmReceiver);
		//ret = MSG_task_send(comm_task,target_mailbox_name);
		if(ret != MSG_OK)
		{
			printf("EROOR\n");
			return 0;
		}
		double end_time = MSG_get_clock();
		printf("send from %s on %s TO %s on %s : %lf seconds\n", MSG_host_get_name(vmSender),MSG_host_get_name(MSG_vm_get_pm(vmSender)),
			 MSG_host_get_name(vmReceiver), MSG_host_get_name(MSG_vm_get_pm(vmReceiver)),(end_time - start_time));
	}

*/

#endif // __UTILITY_H__