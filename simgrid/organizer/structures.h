#ifndef __STRUCTURES_H__
#define __STRUCTURES_H__

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