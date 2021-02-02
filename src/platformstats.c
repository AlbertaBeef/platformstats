/******************************************************************************
* Copyright (C) 2019 - 2020 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/

/******************************************************************************/
/***************************** Include Files *********************************/
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/sysinfo.h>

#include "platformstats.h"
#include "utils.h"

/************************** Function Definitions *****************************/
/*****************************************************************************/
/*
*
* This API opens /proc/stat file to read information for each CPU and store it in struct
* /proc/stat displays the following columns of information for any CPU
* user: time spent on processes executing in user mode with normal priority
* nice: time spent on processes executing in user mode with "niced" priority
* system: time spent on processes executing in kernel mode
* idle: time spent idling (no CPU instructions) while there were no disk I/O requests
* outstanding.
* iowait: time spent idling while there were outstanding disk I/O requests.
* irq: time spent servicing interrupt requests.
* softirq: time spent servicing softirq.
*
* @param	cpu_stat: store CPU stats
* @param	cpu_id: CPU id for which the details must be caputred.
*
* @return	None.
*
* @note		Internal API only.
*
******************************************************************************/
int get_stats(struct cpustat *cpu_stat, int cpu_id)
{
	FILE *fp;

	fp = fopen("/proc/stat", "r");

	if(fp == NULL)
	{
		printf("Unable to open /proc/stat. Returned errono: %d", errno);
		return(errno);
	}
	else
	{
		int lskip;
		char cpun[255];

		lskip = cpu_id+1;
		skip_lines(fp, lskip);

		fscanf(fp,"%s %ld %ld %ld %ld %ld %ld %ld", cpun,
			&(cpu_stat->user), &(cpu_stat->nice), &(cpu_stat->system),
			&(cpu_stat->idle), &(cpu_stat->iowait), &(cpu_stat->irq),
			&(cpu_stat->softirq));

		fclose(fp);
	}

	return(0);
}

/*****************************************************************************/
/*
*
* This API prints CPU stats stored in given structure for particular CPU id 
*
* @param	cpu_stat: struct that stores CPU stats
* @param	cpu_id: CPU id for which the details must be caputred.
*
* @return	None.
*
* @note		Internal API only.
*
******************************************************************************/
int print_cpu_stats(struct cpustat *st, int cpu_id)
{
	printf("CPU%d: %ld %ld %ld %ld %ld %ld %ld\n", cpu_id, (st->user), (st->nice), 
        (st->system), (st->idle), (st->iowait), (st->irq),
        (st->softirq));
	
	return(0);
}

/*****************************************************************************/
/*
*
* This API calculates CPU util in real time, by computing delta at two time instances.
* By default the interval between two time instances is 1s if not specified. 
*
* @param	prev: CPU stats at T0
* @param	curr: CPU stats at T1
*
* @return	cpu_util.
*
* @note		Internal API only.
*
******************************************************************************/
double calculate_load(struct cpustat *prev, struct cpustat *curr)
{
	unsigned long idle_prev, idle_curr, nidle_prev, nidle_curr;
	unsigned long total_prev, total_curr;
	double total_delta, idle_delta, cpu_util; 

	idle_prev=(prev->idle)+(prev->iowait);
	idle_curr=(curr->idle)+(curr->iowait);

	nidle_prev = (prev->user) + (prev->nice) + (prev->system) + (prev->irq) + (prev->softirq);
    	nidle_curr = (curr->user) + (curr->nice) + (curr->system) + (curr->irq) + (curr->softirq);

    	total_prev = idle_prev + nidle_prev;
    	total_curr = idle_curr + nidle_curr;

    	total_delta = (double) total_curr - (double) total_prev;
    	idle_delta = (double) idle_curr - (double) idle_prev;

	cpu_util = (1000 * (total_delta - idle_delta) / total_delta + 1) / 10;
	
	return (cpu_util);
}

/*****************************************************************************/
/*
*
* This API identifies the number of configured CPUs in the system. For each
* active CPU it reads the CPU stats by calling get_stats and then calculates
* load.
*
* @param	verbose_flag: Enable verbose prints on stdout
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
int print_cpu_utilization(int verbose_flag)
{
	int num_cpus_conf, cpu_id;
	struct cpustat *st0_0, *st0_1;

	num_cpus_conf= get_nprocs_conf();
	cpu_id=0;

	st0_0 = malloc(num_cpus_conf * sizeof (struct cpustat));
	st0_1 = malloc(num_cpus_conf * sizeof (struct cpustat));
	
	if(!st0_0 || !st0_1)
	{
		printf("Unable to allocate memory, malloc failed");
		return(errno);
	}

	printf("\nCPU Utilization\n");
	for(; cpu_id < num_cpus_conf; cpu_id++)
	{
		st0_0[cpu_id].total_util = 0;
		st0_1[cpu_id].total_util = 0;

		get_stats(&st0_0[cpu_id],cpu_id);
	        sleep(1);
		get_stats(&st0_1[cpu_id],cpu_id);
		st0_1[cpu_id].total_util = calculate_load(&st0_0[cpu_id],&st0_1[cpu_id]);
	
		if(verbose_flag)
		{
			printf("cpu_id=%d\nStats at t0\n",cpu_id);
			print_cpu_stats(&st0_0[cpu_id],cpu_id);
			printf("Stats at t1 after 1s\n");
			print_cpu_stats(&st0_1[cpu_id],cpu_id);
		}
		printf("CPU%d\t:     %lf%%\n",cpu_id,st0_1[cpu_id].total_util);
	}

	free(st0_0);
	free(st0_1);

	return(0);
}
/*****************************************************************************/
/*
*
* This API identifies the number of configured CPUs in the system. For each
* active CPU it reads the CPU frequency by opening /proc/cpuinfo.
*
* @param	verbose_flag: Enable verbose prints on stdout
*
* @return	cpu_freq.
*
* @note		Internal API.
*
******************************************************************************/

int get_cpu_frequency(int cpu_id, float* cpu_freq)
{
	FILE *fp;

	fp = fopen("/proc/cpuinfo", "r");
	size_t bytes_read;

	if(fp == NULL)
	{
		printf("Unable to open /proc/stat. Returned errono: %d", errno);
		return(errno);
	}
	else
	{
		skip_lines(fp,(cpu_id*27));
		char buff[500];
		char *match;

		bytes_read=fread(buff,sizeof(char),500,fp);
		fclose(fp);

		if(bytes_read == 0)
		{
			return(0);
		}

		match = strstr(buff,"cpu MHz");
		buff[bytes_read]='\0';
		if(match == NULL)
		{
			printf("match not found");
			return(0);
		}

		sscanf(match,"cpu MHz : %f", cpu_freq);
	}

	return(0);
}

/*****************************************************************************/
/*
*
* This API identifies the number of configured CPUs in the system. For each
* active CPU it reads the CPU frequency by calling get_cpu_freq and prints it.
*
* @param	verbose_flag: Enable verbose prints on stdout
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
int print_cpu_frequency(int verbose_flag)
{
	int num_cpus_conf, cpu_id;
	float cpu_freq;

	num_cpus_conf= get_nprocs_conf();
	cpu_id=0;

	printf("\nCPU Frequency\n");
	for(; cpu_id < num_cpus_conf; cpu_id++)
	{
		get_cpu_frequency(cpu_id,&cpu_freq);
		printf("CPU%d\t:    %f MHz\n",cpu_id,cpu_freq);
	}

	return(0);
}

/*****************************************************************************/
/*
*
* This API scans the following information about physical memory:
* MemTotal: Total usable physical ram
* MemFree: The amount of physical ram, in KB, left unused by the system
* MemAvailable: An estimate on how much memory is available for starting new
* applications, without swapping.It can be timated from MemFree, Active(file),
* Inactive(file), and SReclaimable, as well as the "low"
* watermarks from /proc/zoneinfo.
*
* @param        MemTotal: Total usable physical ram size
* @param        MemFree: amount of RAM left unsused
* @param        MemAvailable: estimate of amount of memory available to start a new
* app
*
* @return       Error code.
*
* @note         Internal API.
*
******************************************************************************/
int get_ram_memory_utilization(unsigned long* MemTotal, unsigned long* MemFree, unsigned long* MemAvailable)
{
	//read first three lines of file
	//print to terminal
	FILE *fp;

        fp = fopen("/proc/meminfo", "r");

        if(fp == NULL)
        {
                printf("Unable to open /proc/stat. Returned errono: %d", errno);
                return(errno);
        }
        else
        {
		char buff[80];

		fscanf(fp," %s %ld",buff,MemTotal);
		skip_lines(fp,1);

		fscanf(fp," %s %ld",buff,MemFree);
		skip_lines(fp,1);

		fscanf(fp, "%s %ld",buff,MemAvailable);

		fclose(fp);
	}

	return(0);

}

/*****************************************************************************/
/*
*
* This API prints the following information about physical memory:
* MemTotal: Total usable physical ram
* MemFree: The amount of physical ram, in KB, left unused by the system
* MemAvailable: An estimate on how much memory is available for starting new
* applications, without swapping.It can be timated from MemFree, Active(file),
* Inactive(file), and SReclaimable, as well as the "low"
* watermarks from /proc/zoneinfo.
*
* @param        verbose_flag: Enable verbose prints
* @param        MemAvailable: estimate of amount of memory available to start a new
* app
*
* @return       Error code.
*
* @note         None.
*
******************************************************************************/
int print_ram_memory_utilization(int verbose_flag)
{
	unsigned long MemTotal=0, MemFree=0, MemAvailable=0;
	int mem_util_ret;

	mem_util_ret = 0;

	mem_util_ret = get_ram_memory_utilization(&MemTotal, &MemFree, &MemAvailable);

	printf("\nRAM Utilization\n");
	printf("MemTotal      :     %ld kB\n",MemTotal);
	printf("MemFree	      :     %ld kB\n", MemFree);
	printf("MemAvailable  :     %ld kB\n\n", MemAvailable);

	return(mem_util_ret);

}

/*****************************************************************************/
/*
*
* This API prints the following information about physical memory:
* CMATotal: Total CMA information
* CMAFree: The CMA alloc free information
*
* @param        verbose_flag: Enable verbose prints
*
* @return       Error code.
*
* @note         Internal API.
*
******************************************************************************/
int get_cma_utilization(unsigned long* CmaTotal, unsigned long* CmaFree)
{
	FILE *fp;

        fp = fopen("/proc/meminfo", "r");

        if(fp == NULL)
        {
                printf("Unable to open /proc/stat. Returned errono: %d", errno);
                return(errno);
        }
        else
        {
		char buff[80];

		skip_lines(fp,37);
		fscanf(fp," %s %ld",buff,CmaTotal);

		skip_lines(fp,1);
		fscanf(fp," %s %ld",buff,CmaFree);

		fclose(fp);
	}

	return(0);

}

/*****************************************************************************/
/*
*
* This API prints the following information about physical memory:
* CMATotal: Total CMA information
* CMAFree: The CMA alloc free information
*
* @param        verbose_flag: Enable verbose prints
*
* @return       Error code.
*
* @note         None.
*
******************************************************************************/
int print_cma_utilization(int verbose_flag)
{
	unsigned long CmaTotal=0, CmaFree=0;
	int cma_util_ret;

	cma_util_ret = 0;

	cma_util_ret = get_cma_utilization(&CmaTotal, &CmaFree);

	printf("\nCMA Mem Utilization\n");
	printf("CmaTotal   :     %ld kB\n",CmaTotal);
	printf("CmaFree    :     %ld kB\n", CmaFree);

	return(cma_util_ret);

}

/*****************************************************************************/
/*
*
* This API scans the following information about physical swap memory:
* SwapTotal: Total usable physical swap memory
* SwapFree: The amount of swap memory free. Memory which has been evicted from RAM, 
* and is temporarily on the disk.
*
* @param        SwapTotal: Total usable physical swap size
* @param        SwapFree: amount of swap memory free
*
* @return       Error code.
*
* @note         Internal API.
*
******************************************************************************/
int get_swap_memory_utilization(unsigned long* SwapTotal, unsigned long* SwapFree)
{
	FILE *fp;

        fp = fopen("/proc/meminfo", "r");

        if(fp == NULL)
        {
                printf("Unable to open /proc/stat. Returned errono: %d", errno);
                return(errno);
        }
        else
        {
		char buff[80];

		skip_lines(fp,14);
		fscanf(fp," %s %ld",buff,SwapTotal);

		skip_lines(fp,1);
		fscanf(fp," %s %ld",buff,SwapFree);

		fclose(fp);
	}

	return(0);

}

/*****************************************************************************/
/*
*
* This API prints the following information about swap memory:
* SwapTotal: Total usable physical swap memory
* SwapFree: The amount of swap memory free. Memory which has been evicted from RAM, 
* and is temporarily on the disk.
*
* @param        verbose_flag: Enable verbose prints
*
* @return       Error code.
*
* @note         None.
*
******************************************************************************/
int print_swap_memory_utilization(int verbose_flag)
{
	unsigned long SwapTotal=0, SwapFree=0;
	int mem_util_ret;

	mem_util_ret = 0;

	mem_util_ret = get_swap_memory_utilization(&SwapTotal, &SwapFree);

	printf("\nSwap Mem Utilization\n");
	printf("SwapTotal    :    %ld kB\n",SwapTotal);
	printf("SwapFree     :    %ld kB\n\n",SwapFree);

	return(mem_util_ret);

}

/*****************************************************************************/
/*
*
* This API reads the sysfs enteries for a given sysfs file
*
* @param	filename: sysfs path
* @param	value: value read from sysfs entry
*
* @return       None
*
* @note         None.
*
******************************************************************************/
int read_sysfs_entry(char* filename, char* value)
{

	FILE *fp;

	fp = fopen(filename,"r");

	if(fp == NULL)
	{
		printf("Unable to open %s\n",filename);
		return(errno);
	}

	fscanf(fp,"%s",value);

	return(0);

}

/*****************************************************************************/
/*
*
* This API returns the number of hwmon devices registered under /sys/class/hwmon
*
* @return       num_hwmon_devices: Number of registered hwmon devices
*
* @note         None.
*
******************************************************************************/
int count_hwmon_reg_devices()
{
	//find number of hwmon devices listed under
	int num_hwmon_devices;
	DIR *d;
	struct dirent *dir;

	num_hwmon_devices = 0;
	d = opendir("/sys/class/hwmon");

	if(!d)
	{
		printf("Unable to open /sys/class/hwmon path\n");
		return(errno);
	}

        while((dir = readdir(d)) != NULL)
        {
		if(strstr(dir->d_name, "hwmon"))
		{
			num_hwmon_devices++;
		}
        }

        closedir(d);

	return(num_hwmon_devices);
}

/*****************************************************************************/
/*
*
* This API returns hwmon_id of the specified device:
*
* @param        name: device name for which hwmon_id needs to be identified
*
* @return       hwmon_id
*
* @note         None.
*
******************************************************************************/
int get_device_hwmon_id(int verbose_flag, char* name)
{
	//find number of hwmon devices listed under
	int num_hwmon_devices,hwmon_id;
	char hwmon_id_str[50];
	char *device_name;
	char *filename;

	filename = malloc(255);
	device_name = malloc(255);

	hwmon_id=-1;

	num_hwmon_devices = count_hwmon_reg_devices();

	for(hwmon_id = 0; hwmon_id < num_hwmon_devices; hwmon_id++)
	{
		sprintf(hwmon_id_str,"%d",hwmon_id);
		strcpy(filename,"/sys/class/hwmon/hwmon");
		strcat(filename,hwmon_id_str);
		strcat(filename,"/name");

		read_sysfs_entry(filename,device_name);

		if(!strcmp(name,device_name))
		{
			return(hwmon_id);
		}

		if(verbose_flag)
		{
			printf("filename %s\n",filename);
			printf("device_name = %s\n",device_name);
		}
	}

	free(filename);
	free(device_name);
	return(-1);
}

/*****************************************************************************/
/*
*
* This API prints the following information about power utilization for ina260:
* in1_input: Voltage input value.
* curr1_input: Current input value.
* power1_input: Instantaneous power use
*
* @param        verbose_flag: Enable verbose prints
*
* @return       Error code.
*
* @note         None.
*
******************************************************************************/
int print_ina260_power_info(int verbose_flag)
{
	int hwmon_id;
	long total_power, total_current, total_voltage;
	FILE *fp;
	char filename[255];
	char hwmon_id_str[255];

	char base_filepath[] = "/sys/class/hwmon/hwmon";

	hwmon_id = get_device_hwmon_id(verbose_flag,"ina260_u14");

	printf("\nPower Utilization\n");
	if(hwmon_id == -1)
	{
		printf("no hwmon device found for irps5401 under /sys/class/hwmon\n");
		return(0);
	}

	//printf("hwmon device found, device_id is %d\n",hwmon_id);

	sprintf(hwmon_id_str,"%d",hwmon_id);
	strcat(base_filepath,hwmon_id_str);

	//if "power" file exists then read power value
	strcpy(filename,base_filepath);
	strcat(filename,"/power1_input");

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&total_power);
	fclose(fp);

	printf("SOM total power    :     %ld mW\n",(total_power)/1000);

	//if "curr" file exists then read curr value
	strcpy(filename,base_filepath);
	strcat(filename,"/curr1_input");

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&total_current);
	fclose(fp);
	printf("SOM total current    :     %ld mA\n",total_current);


	//if "voltage" file exists then read voltage value
	strcpy(filename,base_filepath);
	strcat(filename,"/in1_input");

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&total_voltage);
	fclose(fp);
	printf("SOM total voltage\t:     %ld mV\n",total_voltage);


	return(0);
}

/*****************************************************************************/
/*
*
* This API prints the following information from sysmon driver:
* in1_input: Voltage input value.
* curr1_input: Current input value.
* power1_input: Instantaneous power use
*
* @param        verbose_flag: Enable verbose prints
*
* @return       Error code.
*
* @note         None.
*
******************************************************************************/
int print_sysmon_power_info(int verbose_flag)
{
	int hwmon_id;
	FILE *fp;
	char filename[255];
	char hwmon_id_str[255];
	long LPD_TEMP, FPD_TEMP, PL_TEMP;
	long VCC_PSPLL, PL_VCCINT, VOLT_DDRS, VCC_PSINTFP, VCC_PS_FPD;
	long PS_IO_BANK_500, VCC_PS_GTR, VTT_PS_GTR;

	char base_filepath[] = "/sys/class/hwmon/hwmon";

	hwmon_id = get_device_hwmon_id(verbose_flag,"ams");

	if(hwmon_id == -1)
	{
		printf("no hwmon device found for ir38060 under /sys/class/hwmon\n");
		return(0);
	}

	//printf("hwmon device found, device_id is %d \n",hwmon_id);

	sprintf(hwmon_id_str,"%d",hwmon_id);
	strcat(base_filepath,hwmon_id_str);

	//Print temperature values
	strcpy(filename,base_filepath);
	strcat(filename,"/temp1_input");

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&LPD_TEMP);
	fclose(fp);

	//FPD temp
	strcpy(filename,base_filepath);
	strcat(filename,"/temp2_input");

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&FPD_TEMP);
	fclose(fp);

	//PL temp
	strcpy(filename,base_filepath);
	strcat(filename,"/temp3_input");

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&PL_TEMP);
	fclose(fp);

	//VCC_PSPLL
	strcpy(filename,base_filepath);
	strcat(filename,"/in1_input");

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&VCC_PSPLL);
	fclose(fp);

	//PL_VCCINT
	strcpy(filename,base_filepath);
	strcat(filename,"/in3_input");

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&PL_VCCINT);
	fclose(fp);

	//VOLT_DDRS
	strcpy(filename,base_filepath);
	strcat(filename,"/in6_input");

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&VOLT_DDRS);
	fclose(fp);

	//VCC_PSINTFP
	strcpy(filename,base_filepath);
	strcat(filename,"/in7_input");

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&VCC_PSINTFP);
	fclose(fp);

	//VCC_PS_FPD
	strcpy(filename,base_filepath);
	strcat(filename,"/in9_input");

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&VCC_PS_FPD);
	fclose(fp);

	// PS_IO_BANK_500
	strcpy(filename,base_filepath);
	strcat(filename,"/in13_input");

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&PS_IO_BANK_500);
	fclose(fp);

	//VCC_PS_GTR
	strcpy(filename,base_filepath);
	strcat(filename,"/in16_input");

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&VCC_PS_GTR);
	fclose(fp);

	//VTT_PS_GTR
	strcpy(filename,base_filepath);
	strcat(filename,"/in17_input");

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&VTT_PS_GTR);
	fclose(fp);

	printf("AMS CTRL\n");
	printf("System PLLs voltage measurement, VCC_PSLL   		:     %ld mV\n",VCC_PSPLL);
	printf("PL internal voltage measurement, VCC_PSBATT 		:     %ld mV\n",PL_VCCINT);
	printf("Voltage measurement for six DDR I/O PLLs, VCC_PSDDR_PLL :     %ld mV\n",VOLT_DDRS);
	printf("VCC_PSINTFP_DDR voltage measurement         		:     %ld mV\n\n",VCC_PSINTFP);

	printf("PS Sysmon\n");
	printf("LPD temperature measurement 		    		:     %ld C\n",(LPD_TEMP)/1000);
	printf("FPD temperature measurement (REMOTE)  		    		:     %ld C\n",(FPD_TEMP)/1000);
	printf("VCC PS FPD voltage measurement (supply 2)   		:     %ld mV\n",VCC_PS_FPD);
	printf("PS IO Bank 500 voltage measurement (supply 6)		:     %ld mV\n",PS_IO_BANK_500);
	printf("VCC PS GTR voltage   					:     %ld mV\n",VCC_PS_GTR);
	printf("VTT PS GTR voltage    					:     %ld mV\n\n",VTT_PS_GTR);

	printf("PL Sysmon\n");
	printf("PL temperature    					:     %ld C\n",(PL_TEMP)/1000);

	return(0);
}

/*****************************************************************************/
/*
 * *
 * * This API prints the following information about power utilization for ina260:
 * * in1_input: Voltage input value (ie. 12V)
 * * in2_input: Voltage output value (ie. 5V)
 * * curr1_input: Current input value.
 * * power1_input: Instantaneous power use
 * * temp1_input: Temperature
 * *
 * * @param        verbose_flag: Enable verbose prints
 * *
 * * @return       Error code.
 * *
 * * @note         None.
 * *
 * ******************************************************************************/
int print_ultra96v2_power_info(int verbose_flag)
{
	int hwmon_id;
	long total_power, total_current, total_voltage;
	long sbc_temp;
	FILE *fp;
	char filename[255];
	char hwmon_id_str[255];

	char base_filepath[] = "/sys/class/hwmon/hwmon";

	hwmon_id = get_device_hwmon_id(verbose_flag,"ir38060");

	printf("\nPower Utilization for Ultra96-V2:\n");
	if(hwmon_id == -1)
	{
		printf("no hwmon device found for ir38060 under /sys/class/hwmon\n");
		return(0);
	}

	//printf("hwmon device found, device_id is %d\n",hwmon_id);


	sprintf(hwmon_id_str,"%d",hwmon_id);
	strcat(base_filepath,hwmon_id_str);

	//if "power" file exists then read power value
	strcpy(filename,base_filepath);
	strcat(filename,"/power1_input");
	
	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&total_power);
	fclose(fp);


	//if "curr" file exists then read curr value
	strcpy(filename,base_filepath);
	strcat(filename,"/curr1_input");

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&total_current);
	fclose(fp);


	//if "voltage" file exists then read voltage value
	strcpy(filename,base_filepath);
	strcat(filename,"/in2_input");

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&total_voltage);
	fclose(fp);

	//SBC temp
	strcpy(filename,base_filepath);
	strcat(filename,"/temp1_input");

	fp = fopen(filename,"r");
	if(fp == NULL)
	{
		printf("unable to open %s\n",filename);
	}

	fscanf(fp,"%ld",&sbc_temp);
	fclose(fp);

	printf("SBC total power    :     %ld mW\n",(total_power)/1000);
	printf("SBC total current  :     %ld mA\n",total_current);
	printf("SBC total voltage  :     %ld mV\n",total_voltage);
	printf("SBC temperature    :     %ld C\n",(sbc_temp)/1000); 

	return(0);
}


struct pmbus_info pmbus_ultra96v2[] = 
{
   //    device   address           name    label         alias  unit  division
   //  ir38060-i2c-6-45
   {  "ir38060", "6-0045",            "", "pout1", "         5V", "mW", 1000 },
   {  "ir38060", "6-0045",            "", "iout1", "         5V", "mA",    1 },
   {  "ir38060", "6-0045",            "", "iout1", "         5V", "mV",    1 },
   {  "ir38060", "6-0045", "temp1_input", "temp1", "Temperature",  "C", 1000 },
   // irps5401-i2c-6-43
   { "irps5401", "6-0043",            "", "pout1", "     VCCAUX", "mW", 1000 },
   { "irps5401", "6-0043",            "", "pout2", "  VCCO 1.2V", "mW", 1000 },
   { "irps5401", "6-0043",            "", "pout3", "  VCCO 1.1V", "mW", 1000 },
   { "irps5401", "6-0043",            "", "pout4", "     VCCINT", "mW", 1000 },
   { "irps5401", "6-0043",            "", "pout5", "    3.3V DP", "mW", 1000 },
   { "irps5401", "6-0043", "temp1_input", "temp1", "Temperature",  "C", 1000 },
   // irps5401-i2c-6-44
   { "irps5401", "6-0044",            "", "pout1", "   VCCPSAUX", "mW", 1000 },
   { "irps5401", "6-0044",            "", "pout2", "   PSINT_LP", "mW", 1000 },
   { "irps5401", "6-0044",            "", "pout3", "  VCCO 3.3V", "mW", 1000 },
   { "irps5401", "6-0044",            "", "pout4", "   PSINT_FP", "mW", 1000 },
   { "irps5401", "6-0044",            "", "pout5", " PSPLL 1.2V", "mW", 1000 },
   { "irps5401", "6-0044", "temp1_input", "temp1", "Temperature",  "C", 1000 },
   //
   { "", "", "", "", "", 1 } 
};

struct pmbus_info pmbus_uz7ev_evcc[] =
{
   //    device   address           name    label                        alias  unit  division
   //  ir38063-i2c-6-4c
   {  "ir38063", "6-004c",            "", "pout1", "              Carrier 3V3", "mW", 1000 },
   //  ir38063-i2c-6-4b
   {  "ir38063", "6-004b",            "", "pout1", "              Carrier 1V8", "mW", 1000 },
   // irps5401-i2c-6-4a
   { "irps5401", "6-004a",            "", "pout1", "      Carrier 0V9 MGTAVCC", "mW", 1000 },
   { "irps5401", "6-004a",            "", "pout2", "      Carrier 1V2 MGTAVTT", "mW", 1000 },
   { "irps5401", "6-004a",            "", "pout3", "         Carrier 1V1 HDMI", "mW", 1000 },
 //{ "irps5401", "6-004a",            "", "pout4", "                   Unused", "mW", 1000 },
   { "irps5401", "6-004a",            "", "pout5", "Carrier 1V8 MGTVCCAUX LDO", "mW", 1000 },
   // irps5401-i2c-6-49
   { "irps5401", "6-0049",            "", "pout1", "    Carrier 0V85 MGTRAVCC", "mW", 1000 },
   { "irps5401", "6-0049",            "", "pout2", "         Carrier 1V8 VCCO", "mW", 1000 },
   { "irps5401", "6-0049",            "", "pout3", "         Carrier 3V3 VCCO", "mW", 1000 },
   { "irps5401", "6-0049",            "", "pout4", "          Carrier 5V MAIN", "mW", 1000 },
   { "irps5401", "6-0049",            "", "pout5", " Carrier 1V8 MGTRAVTT LDO", "mW", 1000 },
   { "irps5401", "6-0049", "temp1_input", "temp1", "              Temperature",  "C", 1000 },
   //  ir38063-i2c-6-48
   {  "ir38063", "6-0048",            "", "pout1", "          SOM 0V85 VCCINT", "mW", 1000 },
   // irps5401-i2c-6-47
   { "irps5401", "6-0047",            "", "pout1", "           SOM 1V8 VCCAUX", "mW", 1000 },
   { "irps5401", "6-0047",            "", "pout2", "                  SOM 3V3", "mW", 1000 },
   { "irps5401", "6-0047",            "", "pout3", "           SOM 0V9 VCUINT", "mW", 1000 },
   { "irps5401", "6-0047",            "", "pout4", "       SOM 1V2 VCCO_HP_66", "mW", 1000 },
   { "irps5401", "6-0047",            "", "pout5", "    SOM 1V8 PSDDR_PLL LDO", "mW", 1000 },
   { "irps5401", "6-0047", "temp1_input", "temp1", "              Temperature",  "C", 1000 },
   // irps5401-i2c-6-46
   { "irps5401", "6-0046",            "", "pout1", "        SOM 1V2 VCCO_PSIO", "mW", 1000 },
   { "irps5401", "6-0046",            "", "pout2", "     SOM 0V85 VCC_PSINTLP", "mW", 1000 },
   { "irps5401", "6-0046",            "", "pout3", "  SOM 1V2 VCCO_PSDDR4_504", "mW", 1000 },
   { "irps5401", "6-0046",            "", "pout4", "     SOM 0V85 VCC_PSINTFP", "mW", 1000 },
   { "irps5401", "6-0046",            "", "pout5", "    SOM 1V2 VCC_PSPLL LDO", "mW", 1000 },
   { "irps5401", "6-0046", "temp1_input", "temp1", "              Temperature",  "C", 1000 },
   //
   { "", "", "", "", "", 1 } 
};

struct pmbus_info pmbus_uz3eg_xxx[] =
{
   //    device   address           name    label         alias  unit  division
   // irps5401-i2c-6-43
   { "irps5401", "6-0043",            "", "pout1", "       PSIO", "mW", 1000 },
   { "irps5401", "6-0043",            "", "pout2", "     VCCAUX", "mW", 1000 },
   { "irps5401", "6-0043",            "", "pout3", "    PSINTLP", "mW", 1000 },
   { "irps5401", "6-0043",            "", "pout4", "    PSINTFP", "mW", 1000 },
   { "irps5401", "6-0043",            "", "pout5", "      PSPLL", "mW", 1000 },
   { "irps5401", "6-0043", "temp1_input", "temp1", "Temperature",  "C", 1000 },
   // irps5401-i2c-6-44
   { "irps5401", "6-0044",            "", "pout1", "     PSDDR4", "mW", 1000 },
   { "irps5401", "6-0044",            "", "pout2", "     INT_IO", "mW", 1000 },
   { "irps5401", "6-0044",            "", "pout3", "       3.3V", "mW", 1000 },
   { "irps5401", "6-0044",            "", "pout4", "        INT", "mW", 1000 },
   { "irps5401", "6-0044",            "", "pout5", "   PSDDRPLL", "mW", 1000 },
   { "irps5401", "6-0044", "temp1_input", "temp1", "Temperature",  "C", 1000 },
   // irps5401-i2c-6-45
   { "irps5401", "6-0045",            "", "pout1", "    MGTAVCC", "mW", 1000 },
   { "irps5401", "6-0045",            "", "pout2", "         5V", "mW", 1000 },
   { "irps5401", "6-0045",            "", "pout3", "       3.3V", "mW", 1000 },
   { "irps5401", "6-0045",            "", "pout4", "  VCCO 1.8V", "mW", 1000 },
   { "irps5401", "6-0045",            "", "pout5", "    MGTAVTT", "mW", 1000 },
   { "irps5401", "6-0045", "temp1_input", "temp1", "Temperature",  "C", 1000 },
   //
   { "", "", "", "", "", 1 } 
};



int get_pmbus_device_filename(int verbose_flag, struct pmbus_info *pInfo, char *filename)
{
	//find number of hwmon devices listed under
	int num_hwmon_devices,hwmon_id;
	char hwmon_id_str[50];
	char *device_name;
	FILE *fp;

  DIR *d;
	struct dirent *dir;
  char reg_label[255];
  char reg_name[255];

	//filename = malloc(255);
	device_name = malloc(255);

	hwmon_id=-1;

	num_hwmon_devices = count_hwmon_reg_devices();

	for(hwmon_id = 0; hwmon_id < num_hwmon_devices; hwmon_id++)
	{
 		sprintf(hwmon_id_str,"%d",hwmon_id);

    // use "/sys/class/hwmon/hwmon[hwmon_id]/device/driver/[address]/name" to find matching devide name   
		strcpy(filename,"/sys/class/hwmon/hwmon");
		strcat(filename,hwmon_id_str);
    strcat(filename,"/device/driver/");
    strcat(filename,pInfo->address);
		strcat(filename,"/name");

		//read_sysfs_entry(filename,device_name);
  	fp = fopen(filename,"r");
  	if(fp == NULL)
	  {
		  //printf("Unable to open %s\n",filename);
		  continue;
	  }
   	fscanf(fp,"%s",device_name);
    fclose(fp);
		if(verbose_flag)
		{
			printf("\t%s => %s\n",filename, device_name);
		}

    // redundant check ... 
		if(strcmp(pInfo->device,device_name))
		{
      continue;
    }
    
    // use "/sys/class/hwmon/hwmon[]/device/driver/[address]/hwmon[hwmon_id]" to find correct hwmon_id   
		strcpy(filename,"/sys/class/hwmon/hwmon");
		strcat(filename,hwmon_id_str);
    strcat(filename,"/device/driver/");
    strcat(filename,pInfo->address);
		strcat(filename,"/hwmon");
    //
    d = opendir(filename);
    if(!d)
    {
      printf("Unable to open %s path\n", filename);
    	return(errno);
   	}
    dir = readdir(d); // .
    dir = readdir(d); // ..
    dir = readdir(d); // hwmon[id]
    if (dir == NULL)
    {
      printf("Unable to read %s directory\n", filename);
      return(errno);
    }
		if(verbose_flag)
		{
			printf("\t%s => %s\n",filename, dir->d_name);
		}
   	sscanf( &(dir->d_name[5]), "%d", &hwmon_id);
   	sprintf(hwmon_id_str,"%d",hwmon_id);
    
    {
      if ( strcmp(pInfo->name,"") )
      {
          // If name is specified, create the full filename
          strcpy(filename,"/sys/class/hwmon/hwmon");
          strcat(filename,hwmon_id_str);
          strcat(filename,"/");
          strcat(filename,pInfo->name);
          
          if (verbose_flag)
          {
            printf("\t%s@%s-%s => %s\n", pInfo->device, pInfo->address, pInfo->label, filename );
          }
          return hwmon_id;
      }
      else
      {
        if (verbose_flag)
        {
          printf("\tSearching for name that matches label %s\n", pInfo->label );
        }
        // search for name with matching label
    	  strcpy(filename,"/sys/class/hwmon/hwmon");
        strcat(filename,hwmon_id_str);
    	  d = opendir(filename);
        if(!d)
        {
      		printf("Unable to open %s path\n", filename);
    	  	return(errno);
    	  }
        strcat(reg_label,"");
        while((dir = readdir(d)) != NULL)
        {
    		  if(strstr(dir->d_name, "label"))
    		  {
            strcpy(filename,"/sys/class/hwmon/hwmon");
            strcat(filename,hwmon_id_str);
            strcat(filename,"/");
            strcat(filename,dir->d_name);
        	  fp = fopen(filename,"r");
    	      if(fp == NULL)
          	{
    		      printf("unable to open %s\n",filename);
              continue;
    	      }
    	      fscanf(fp,"%s",&reg_label);
      	    fclose(fp);
            if ( !strcmp(reg_label,pInfo->label) )
            {
              //printf("%s => %s\n", reg_label, dir->d_name );
              // Now that we found correct label in [reg]_label, use [reg]_input to read values
              strcpy(reg_name,dir->d_name);
              strcpy(&reg_name[strlen(dir->d_name)-5],"input");
              //printf("%s => %s\n", reg_label, reg_name );
              
              // Remember name for next iteration
              pInfo->name = malloc( strlen(reg_name)+1 );
              strcpy(pInfo->name,reg_name);
              
              // Create the full filename
              strcpy(filename,"/sys/class/hwmon/hwmon");
              strcat(filename,hwmon_id_str);
              strcat(filename,"/");
              strcat(filename,reg_name);
              
              if (verbose_flag)
              {
                printf("\t%s@%s-%s => %s\n", pInfo->device, pInfo->address, pInfo->label, filename );
              }
              return hwmon_id;
            }
    		  }
        }
        closedir(d);
      }
		}

	}

	free(device_name);
	return(-1);
}

void print_pmbus_info(int verbose_flag, struct pmbus_info pmbus_list[])
{
	int hwmon_id;
	long pmbus_value;
	FILE *fp;
	char filename[255];
	char hwmon_id_str[255];

	char base_filepath[] = "/sys/class/hwmon/hwmon";

	printf("\nPower Utilization:\n");

  int i = 0;
  while ( strcmp(pmbus_list[i].device,"") ) 
  {
    if (verbose_flag)
    {
      printf("[%d] %s,%s,%s,%s,%s\n", i, pmbus_list[i].device, pmbus_list[i].address, pmbus_list[i].label, pmbus_list[i].name, pmbus_list[i].unit );
    }
    hwmon_id = get_pmbus_device_filename(verbose_flag,&pmbus_list[i],&filename);
    //printf("%d %s\n",hwmon_id,filename);

    if (verbose_flag)
    {
      printf("\t%s@%s-%s => %s\n", pmbus_list[i].device,pmbus_list[i].address,pmbus_list[i].label, filename );
    }

	
	  fp = fopen(filename,"r");
	  if(fp == NULL)
  	{
		  printf("unable to open %s\n",filename);
	  }
	  fscanf(fp,"%ld",&pmbus_value);
  	fclose(fp);
    printf("\t%s@%s-%s (%s) = %ld %s\n", pmbus_list[i].device, pmbus_list[i].address, pmbus_list[i].label, pmbus_list[i].alias, (pmbus_value/pmbus_list[i].division), pmbus_list[i].unit);

    i++;
  }
}
/*****************************************************************************/
/*
*
* This API prints the following information about power utilization for the system:
*
* @param        verbose_flag: Enable verbose prints
*
* @return       Error code.
*
* @note         None.
*
******************************************************************************/
int print_power_utilization(int verbose_flag)
{
  FILE *fp;
  char hostname[128] = "";
  
  fp = popen("hostname", "r");
  if (fp != NULL) {
    if ( fgets(hostname, sizeof(hostname), fp) != NULL ) {
      if (verbose_flag)
      {
        printf("hostname=%s\n", hostname);
      }
    }
    pclose(fp);
  }

	//print_ina260_power_info(verbose_flag);
	//print_sysmon_power_info(verbose_flag);
  
  if ( strstr(hostname,"u96v2") )
  {
    if (verbose_flag)
    {
      printf("Ultra96-V2\n");
    }
	  //print_ultra96v2_power_info(verbose_flag);
    print_pmbus_info(verbose_flag,pmbus_ultra96v2);
  }
  if ( strstr(hostname,"uz7ev") )
  {
    if (verbose_flag)
    {
      printf("UltraZed-7EV-EVCC\n");
    }
    print_pmbus_info(verbose_flag,pmbus_uz7ev_evcc);
  }
  if ( strstr(hostname,"uz3eg") )
  {
    if (verbose_flag)
    {
      printf("UltraZed-3EG\n");
    }
    print_pmbus_info(verbose_flag,pmbus_uz3eg_xxx);
  }

	return(0);
}

/*****************************************************************************/
/*
*
* This API calls all other APIs that read, compute and print different platform
* stats
*
* @param        verbose_flag: Enable verbose prints on stdout
* and printed
*
* @return       None.
*
* @note         None.
*
******************************************************************************/
void print_all_stats(int verbose_flag)
{

	print_cpu_utilization(verbose_flag);

	print_ram_memory_utilization(verbose_flag);

	print_swap_memory_utilization(verbose_flag);

	print_power_utilization(verbose_flag);

	print_cma_utilization(verbose_flag);

	print_cpu_frequency(verbose_flag);
}
