#ifndef __STRUCTURES_H__
#define __STRUCTURES_H__

#define NUMBER_OF_CLUSTERS 1
#define MAX_VM_MEMORY 1e9
#define MAX_VM_NET 1.25e8
#define PM_CAPACITY 4
#define IS_ENABLE_GUEST_LOAD_BALANCE 0
#define ALPHA 0.1f

typedef struct PMs_data
{
	const char *PM_name;
	int numberOfVMs;
	int VMs[4];
}DATA_OF_PM;


typedef enum HOST_ASSIGN_POLICY
{
	INHAB_MIG = 0,
	MIN_COMM
}host_assign_policy;

typedef struct slow_down_t
{
	double expected_time;
	double actual_time;
} SlowDown;

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

typedef struct ResourceNeed
{
	double cpu;
	double memory;
	double net;
	double expected_time;
	double cpu_time;
} resource_need;

#endif // __STRUCTURES_H__