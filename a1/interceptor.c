#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <asm/current.h>
#include <asm/ptrace.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <asm/unistd.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/syscalls.h>
#include "interceptor.h"

MODULE_DESCRIPTION("My kernel module");
MODULE_AUTHOR("Your name here ...");
MODULE_LICENSE("GPL");

//----- System Call Table Stuff ------------------------------------
/* Symbol that allows access to the kernel system call table */
extern void* sys_call_table[];

/* The sys_call_table is read-only => must make it RW before replacing a syscall */
void set_addr_rw(unsigned long addr) {

	unsigned int level;
	pte_t *pte = lookup_address(addr, &level);

	if (pte->pte &~ _PAGE_RW) pte->pte |= _PAGE_RW;

}

/* Restores the sys_call_table as read-only */
void set_addr_ro(unsigned long addr) {

	unsigned int level;
	pte_t *pte = lookup_address(addr, &level);

	pte->pte = pte->pte &~_PAGE_RW;

}
//-------------------------------------------------------------


//----- Data structures and bookkeeping -----------------------
/**
 * This block contains the data structures needed for keeping track of
 * intercepted system calls (including their original calls), pid monitoring
 * synchronization on shared data, etc.
 * It's highly unlikely that you will need any globals other than these.
 */

/* List structure - each intercepted syscall may have a list of monitored pids */
struct pid_list {
	pid_t pid;
	struct list_head list;
};


/* Store info about intercepted/replaced system calls */
typedef struct {

	/* Original system call */
	asmlinkage long (*f)(struct pt_regs);

	/* Status: 1=intercepted, 0=not intercepted */
	int intercepted;

	/* Are any PIDs being monitored for this syscall? */
	int monitored;	// 0 = none, 1 = some, 2 = all
	/* List of monitored PIDs */
	int listcount;
	struct list_head my_list;
} mytable;

/* An entry for each system call in this "metadata" table */
mytable table[NR_syscalls];

/* Access to the system call table and your metadata table must be synchronized */
spinlock_t my_table_lock = SPIN_LOCK_UNLOCKED;
spinlock_t sys_call_table_lock = SPIN_LOCK_UNLOCKED;
//-------------------------------------------------------------


//----------LIST OPERATIONS------------------------------------
/**
 * These operations are meant for manipulating the list of pids 
 * Nothing to do here, but please make sure to read over these functions 
 * to understand their purpose, as you will need to use them!
 */

/**
 * Add a pid to a syscall's list of monitored pids. 
 * Returns -ENOMEM if the operation is unsuccessful.
 */
static int add_pid_sysc(pid_t pid, int sysc)
{
	struct pid_list *ple=(struct pid_list*)kmalloc(sizeof(struct pid_list), GFP_KERNEL);

	if (!ple)
		return -ENOMEM;

	INIT_LIST_HEAD(&ple->list);
	ple->pid=pid;

	list_add(&ple->list, &(table[sysc].my_list));
	table[sysc].listcount++;

	return 0;
}

/**
 * Remove a pid from a system call's list of monitored pids.
 * Returns -EINVAL if no such pid was found in the list.
 */
static int del_pid_sysc(pid_t pid, int sysc)
{
	struct list_head *i;
	struct pid_list *ple;

	list_for_each(i, &(table[sysc].my_list)) {

		ple=list_entry(i, struct pid_list, list);
		if(ple->pid == pid) {

			list_del(i);
			kfree(ple);

			table[sysc].listcount--;
			/* If there are no more pids in sysc's list of pids, then
			 * stop the monitoring only if it's not for all pids (monitored=2) */
			if(table[sysc].listcount == 0 && table[sysc].monitored == 1) {
				table[sysc].monitored = 0;
			}

			return 0;
		}
	}

	return -EINVAL;
}

/**
 * Remove a pid from all the lists of monitored pids (for all intercepted syscalls).
 * Returns -1 if this process is not being monitored in any list.
 */
static int del_pid(pid_t pid)
{
	struct list_head *i, *n;
	struct pid_list *ple;
	int ispid = 0, s = 0;

	for(s = 1; s < NR_syscalls; s++) {

		list_for_each_safe(i, n, &(table[s].my_list)) {

			ple=list_entry(i, struct pid_list, list);
			if(ple->pid == pid) {

				list_del(i);
				ispid = 1;
				kfree(ple);

				table[s].listcount--;
				/* If there are no more pids in sysc's list of pids, then
				 * stop the monitoring only if it's not for all pids (monitored=2) */
				if(table[s].listcount == 0 && table[s].monitored == 1) {
					table[s].monitored = 0;
				}
			}
		}
	}

	if (ispid) return 0;
	return -1;
}

/**
 * Clear the list of monitored pids for a specific syscall.
 */
static void destroy_list(int sysc) {

	struct list_head *i, *n;
	struct pid_list *ple;

	list_for_each_safe(i, n, &(table[sysc].my_list)) {

		ple=list_entry(i, struct pid_list, list);
		list_del(i);
		kfree(ple);
	}

	table[sysc].listcount = 0;
	table[sysc].monitored = 0;
}

/**
 * Check if two pids have the same owner - useful for checking if a pid 
 * requested to be monitored is owned by the requesting process.
 * Remember that when requesting to start monitoring for a pid, only the 
 * owner of that pid is allowed to request that.
 */
static int check_pids_same_owner(pid_t pid1, pid_t pid2) {

	struct task_struct *p1 = pid_task(find_vpid(pid1), PIDTYPE_PID);
	struct task_struct *p2 = pid_task(find_vpid(pid2), PIDTYPE_PID);
	if(p1->real_cred->uid != p2->real_cred->uid)
		return -EPERM;
	return 0;
}

/**
 * Check if a pid is already being monitored for a specific syscall.
 * Returns 1 if it already is, or 0 if pid is not in sysc's list.
 */
static int check_pid_monitored(int sysc, pid_t pid) {

	struct list_head *i;
	struct pid_list *ple;

	list_for_each(i, &(table[sysc].my_list)) {

		ple=list_entry(i, struct pid_list, list);
		if(ple->pid == pid) 
			return 1;
		
	}
	return 0;	
}
//----------------------------------------------------------------

//----- Intercepting exit_group ----------------------------------
/**
 * Since a process can exit without its owner specifically requesting
 * to stop monitoring it, we must intercept the exit_group system call
 * so that we can remove the exiting process's pid from *all* syscall lists.
 */  

/** 
 * Stores original exit_group function - after all, we must restore it 
 * when our kernel module exits.
 */
asmlinkage long (*orig_exit_group)(struct pt_regs reg);

/**
 * Our custom exit_group system call.
 *
 * TODO: When a process exits, make sure to remove that pid from all lists.
 * The exiting process's PID can be retrieved using the current variable (current->pid).
 * Don't forget to call the original exit_group.
 *
 * Note: using printk in this function will potentially result in errors!
 *
 */
asmlinkage long my_exit_group(struct pt_regs reg)
{
	// delete the exiting process's pid from *all* syscall lists.
	del_pid(current->pid);

	// Calling the orig_exit_group function.
	return (*orig_exit_group)(reg);

	// return 0;
}
//----------------------------------------------------------------



/** 
 * This is the generic interceptor function.
 * It should just log a message and call the original syscall.
 * 
 * TODO: Implement this function. 
 * - Check first to see if the syscall is being monitored for the current->pid. 
 * - Recall the convention for the "monitored" flag in the mytable struct: 
 *     monitored=0 => not monitored
 *     monitored=1 => some pidspids are monitored, check the corresponding my_list
 *     monitored=2 => all  are monitored for this syscall
 * - Use the log_message macro, to log the system call parameters!
 *     Remember that the parameters are passed in the pt_regs registers.
 *     The syscall parameters are found (in order) in the 
 *     ax, bx, cx, dx, si, di, and bp registers (see the pt_regs struct).
 * - Don't forget to call the original system call, so we allow processes to proceed as normal.
 */
asmlinkage long interceptor(struct pt_regs reg) {

	// lock access to my_table
	spin_lock(&my_table_lock);

	// Check if the syscall is being monitored for the current->pid
	if (check_pid_monitored(reg.ax, current->pid) == 0) {
		// unlock access to my_table
		spin_unlock(&my_table_lock);
		return 1;
	}

	// unlock access to my_table
	// spin_unlock(&my_table_lock);

	// lock access to my_table
	// spin_lock(&my_table_lock);

	// if syscall.monitored == 2 then check if the pid is not in the pid list
	if (table[reg.ax].monitored == 2) {

		if (check_pid_monitored(reg.ax, current->pid) == 0)
		{
			// unlock access to my_table
			// spin_unlock(&my_table_lock);
			// Log the message
			log_message(current->pid, reg.ax, reg.bx, reg.cx, reg.dx, reg.si, reg.di, reg.bp);
		}


	// else if monitored    == 1 then check if the pid is in the pid list
	} else if (table[reg.ax].monitored == 1) {

		if (check_pid_monitored(reg.ax, current->pid) == 1)
		{
			// unlock access to my_table
			// spin_unlock(&my_table_lock);
			// Log the message
			log_message(current->pid, reg.ax, reg.bx, reg.cx, reg.dx, reg.si, reg.di, reg.bp);
		}


	}

	// store the org call
	int ret_val = (table[reg.ax].f)(reg);

	// unlock access to my_table
	spin_unlock(&my_table_lock);

	// Call the original system call
	return ret_val;
}

/**
 * My system call - this function is called whenever a user issues a MY_CUSTOM_SYSCALL system call.
 * When that happens, the parameters for this system call indicate one of 4 actions/commands:
 *      - REQUEST_SYSCALL_INTERCEPT to intercept the 'syscall' argument
 *      - REQUEST_SYSCALL_RELEASE to de-intercept the 'syscall' argument
 *      - REQUEST_START_MONITORING to start monitoring for 'pid' whenever it issues 'syscall' 
 *      - REQUEST_STOP_MONITORING to stop monitoring for 'pid'
 *      For the last two, if pid=0, that translates to "all pids".
 * 
 * TODO: Implement this function, to handle all 4 commands correctly.
 * - Check for correct context of commands (-EINVAL):
 * - Check for -EBUSY conditions:
 * - If a pid cannot be added to a monitored list, due to no memory being available,
 *   an -ENOMEM error code should be returned.
 *
 *   NOTE: The order of the checks may affect the tester, in case of several error conditions
 *   in the same system call, so please be careful!
 *
 * - Make sure to keep track of all the metadata on what is being intercepted and monitored.
 *   Use the helper functions provided above for dealing with list operations.
 *
 * - Whenever altering the sys_call_table, make sure to use the set_addr_rw/set_addr_ro functions
 *   to make the system call table writable, then set it back to read-only. 
 *   For example: set_addr_rw((unsigned long)sys_call_table);
 *   Also, make sure to save the original system call (you'll need it for 'interceptor' to work correctly).
 * 
 * - Make sure to use synchronization to ensure consistency of shared data structures.
 *   Use the sys_call_table_lock and my_table_lock to ensure mutual exclusion for accesses 
 *   to the system call table and the lists of monitored pids. Be careful to unlock any spinlocks 
 *   you might be holding, before you exit the function (including error cases!).  
 */
asmlinkage long my_syscall(int cmd, int syscall, int pid) {

	// if (pid == 1)
	// {
	// 	printk("DEBUG: syscall is %d and uid is %d", syscall, current_uid());
	// }

	// the syscall must not be negative, not > NR_syscalls-1, and not MY_CUSTOM_SYSCALL
	if (0 > syscall || syscall > NR_syscalls-1 || syscall == MY_CUSTOM_SYSCALL)
	{
		return -EINVAL;
	}

	// For the first two commands, we must be root.
	if (cmd == REQUEST_SYSCALL_INTERCEPT || cmd == REQUEST_SYSCALL_RELEASE) 
	{
		if (current_uid() != 0) { return -EPERM; }
	}

	if (cmd == REQUEST_START_MONITORING || cmd == REQUEST_STOP_MONITORING) 
	{	
		// calling process is not root
		if (current_uid() != 0) {

			// check if the 'pid' requested is owned by the calling process 
			if (check_pids_same_owner(pid, current->pid) != 0) { 
				printk("DEBUG: return eperm\n");
				return -EPERM;
			}
			// if 'pid' is 0 and calling process is not root, then access is denied
			if (pid == 0) { return -EINVAL; }

		}

		// // pid cannot be a negative integer and it must be an existing pid.
		// if (pid < 0 || pid_task(find_vpid(pid), PIDTYPE_PID) == NULL) {


		// 	printk("DEBUG: not a valid task \n ");


		// 	return -EINVAL;
		// }

		if (cmd == REQUEST_START_MONITORING || cmd == REQUEST_STOP_MONITORING)
		{

			if(pid < 0) return -EINVAL;
			else if(pid > 0 && pid_task(find_vpid(pid), PIDTYPE_PID) == NULL) return -EINVAL;

		}

	}

	// lock access to my_table
	spin_lock(&my_table_lock);

	// Cannot de-intercept a system call that has not been intercepted yet.
	if (cmd == REQUEST_SYSCALL_RELEASE && table[syscall].intercepted == 0)
	{	
		spin_unlock(&my_table_lock);
		return -EINVAL;
	}

	/* Cannot stop monitoring for a pid that is not being monitored, 
	*  or if the system call has not been intercepted yet.
	*/
	if (cmd == REQUEST_STOP_MONITORING)
	{
		if (table[syscall].intercepted == 0 || check_pid_monitored(syscall, pid) == 0) {	
			printk("DEBUG: not being monitored intercepted is set to be: %d \n", table[syscall].intercepted);
			spin_unlock(&my_table_lock);
			return -EINVAL;
		}
	}

	// If intercepting a system call that is already intercepted.
	if (cmd == REQUEST_SYSCALL_INTERCEPT && table[syscall].intercepted == 1)
	{	
		spin_unlock(&my_table_lock);
		return -EBUSY;
	}

	// If monitoring a pid that is already being monitored
	if (cmd == REQUEST_START_MONITORING && check_pid_monitored(syscall, pid) == 1) { 
		spin_unlock(&my_table_lock);
		return -EBUSY;
	}

	spin_unlock(&my_table_lock);
	

	if (cmd == REQUEST_SYSCALL_INTERCEPT) {
		
		// printk("Intercept prob");

		// Lock access to kernel system call table.
		spin_lock(&sys_call_table_lock);
		// Update the sys_call_table.
		set_addr_rw((unsigned long) sys_call_table);
		sys_call_table[syscall] = interceptor;
		set_addr_ro((unsigned long) sys_call_table);
		// Unlock the syslock table.
		spin_unlock(&sys_call_table_lock);

		// lock access to my table
		spin_lock(&my_table_lock);

		table[syscall].intercepted = 1;
		
		// unlock access to my table
		spin_unlock(&my_table_lock);


	} else if (cmd == REQUEST_SYSCALL_RELEASE) {
		
		// printk("Release prob");
		// Lock mytable.
		spin_lock(&my_table_lock);

		// Set up "mytable" for a syscall.
		table[syscall].intercepted	= 0;
		
		// destroys pid list and sets listCount and monitored to 0.
		destroy_list(syscall);

		// Unlock  mytable.
		spin_unlock(&my_table_lock);

		// Lock access to kernel system call table.
		spin_lock(&sys_call_table_lock);

		// // Lock mytable.
		// spin_lock(&my_table_lock);

		// Update the sys_call_table.
		set_addr_rw((unsigned long)sys_call_table);
		sys_call_table[syscall] = table[syscall].f;
		set_addr_ro((unsigned long)sys_call_table);

		// // Unlock  mytable.
		// spin_unlock(&my_table_lock);

		// Unlock the syslock table.
		spin_unlock(&sys_call_table_lock);

	} else if (cmd == REQUEST_START_MONITORING)	{

		// case1: pid == 0
			// set monitored = 2
			// del list
		// case2: pid != 0
			// TODO: How to update monitor here.
			// case2.1: monitored == 1 ie monitor a pid
				// add the pid to the pid list for the syscall.
			// case2.2: monitored == 2 ie monitor all pids
			// 	return -EBUSY

		// lock my_table
		spin_lock(&my_table_lock);

		if (pid == 0) {

			// // lock my_table
			// spin_lock(&my_table_lock);

			destroy_list(syscall);
			table[syscall].monitored = 2;

			// // unlock my_table
			// spin_unlock(&my_table_lock);
		
		} else {

			// // lock my_table
			// spin_lock(&my_table_lock);

			if (table[syscall].monitored == 2) {

				spin_unlock(&my_table_lock);
				return -EBUSY;

			} else {

				table[syscall].monitored = 1;
				
				if (add_pid_sysc(pid, syscall) != 0) {

					spin_unlock(&my_table_lock);
					return -ENOMEM;

				}
			}

		}

		spin_unlock(&my_table_lock);

	} else if (cmd == REQUEST_STOP_MONITORING) {

		// case1: pid == 0
			// set monitored = 0
			// del list
		// case2: pid != 0
			// TODO: How to update monitor here.
			// case2.1: monitored == 1 ie monitor a pid
				// delete the pid to the pid list for the syscall.
			// case2.2: monitored == 2 ie monitor all pids
				// add the pid from the pid list.
		// lock my_table
		spin_lock(&my_table_lock);

		if (pid == 0) {

			// // lock my_table
			// spin_lock(&my_table_lock);

			destroy_list(syscall);

			// // unlock my_table
			// spin_unlock(&my_table_lock);
		
		} else {

			// // lock my_table
			// spin_lock(&my_table_lock);

			if (table[syscall].monitored == 1) {

				printk("DEBUG: call being monitored");

				if (del_pid_sysc(pid, syscall) != 0) {

					spin_unlock(&my_table_lock);
					return -ENOMEM;

				}

			} else if (table[syscall].monitored == 2) {

				if (add_pid_sysc(pid, syscall) != 0) {

					spin_unlock(&my_table_lock);
					return -ENOMEM;

				}

			}

		}

		spin_unlock(&my_table_lock);

	}

	return 0;
}

/**
 *
 */
long (*orig_custom_syscall)(void);


/**
 * Module initialization. 
 *
 * TODO: Make sure to:  
 * - Hijack MY_CUSTOM_SYSCALL and save the original in orig_custom_syscall.
 * - Hijack the exit_group system call (__NR_exit_group) and save the original 
 *   in orig_exit_group.
 * - Make sure to set the system call table to writable when making changes, 
 *   then set it back to read only once done.
 * - Perform any necessary initializations for bookkeeping data structures. 
 *   To initialize a list, use 
 *        INIT_LIST_HEAD (&some_list);
 *   where some_list is a "struct list_head". 
 * - Ensure synchronization as needed.
 */
static int init_function(void) {

	// Lock access to kernel system call table
	spin_lock(&sys_call_table_lock);

	// Save original system call
	orig_custom_syscall = sys_call_table[MY_CUSTOM_SYSCALL];

	// Save original exit group
	orig_exit_group = sys_call_table[__NR_exit_group];

	// Set kernel system call table to read-write
	set_addr_rw((unsigned long) sys_call_table);

	// Hijack __NR_exit_group and replace with my_exit_group
	sys_call_table[__NR_exit_group] = my_exit_group;
		
	// Hijack MY_CUSTOM_SYSCALL
	sys_call_table[MY_CUSTOM_SYSCALL] = my_syscall;

	// Set kernel system call table to read only and unlock access to kernel system call table
	set_addr_ro((unsigned long) sys_call_table);

	// unlock sys_call_table
	spin_unlock(&sys_call_table_lock);

	// Lock mytable.
	spin_lock(&my_table_lock);

	// skipping 0 since it points to MY_CUSTOM_SYSCALL
	int i;
	for (i = 0; i < NR_syscalls; i++)
	{
		
		// Set up "mytable" for a syscall.
		table[i].f				= sys_call_table[i];
		table[i].intercepted	= 0;
		table[i].monitored 		= 0;
		table[i].listcount 		= 0;
		INIT_LIST_HEAD (&table[i].my_list);

	}

	// Unlock  mytable.
	spin_unlock(&my_table_lock);

	return 0;
}

/**
 * Module exits. 
 *
 * TODO: Make sure to:  
 * - Restore MY_CUSTOM_SYSCALL to the original syscall.
 * - Restore __NR_exit_group to its original syscall.
 * - Make sure to set the system call table to writable when making changes, 
 *   then set it back to read only once done.
 * - Ensure synchronization, if needed.
 */
static void exit_function(void){
	
	// lock sys_call_table
	spin_lock(&sys_call_table_lock);

	// Set system call table to read write and restore original system call
	set_addr_rw((unsigned long) sys_call_table);
	
	// Restore MY_CUSTOM_SYSCALL to original syscall
	sys_call_table[MY_CUSTOM_SYSCALL] = orig_custom_syscall;
	// Restore __NR_exit_group to original syscall
	sys_call_table[__NR_exit_group] = orig_exit_group;

	// Set system call table to read only
	set_addr_ro((unsigned long) sys_call_table);

	// unlock sys_call_table
	spin_unlock(&sys_call_table_lock);

	// lock  mytable.
	spin_lock(&my_table_lock);

	// lock sys_call_table
	spin_lock(&sys_call_table_lock);

	// skipping 0 since it points to MY_CUSTOM_SYSCALL
	int i;
	for (i = 1; i < NR_syscalls; i++)
	{	
		if (i != __NR_exit_group)
		{
			sys_call_table[i] = table[i].f;
		}
		table[i].intercepted = 0;
		destroy_list(i);

	}

	// Unlock  mytable.
	spin_unlock(&my_table_lock);

	// unlock sys_call_table
	spin_unlock(&sys_call_table_lock);
}

module_init(init_function);
module_exit(exit_function);
