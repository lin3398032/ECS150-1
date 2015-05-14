#include "VirtualMachine.h"
#include "Machine.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <vector>
#include<list>
//for testing 
#include <iostream> 
#include <cstdlib> 
using namespace std;
//VirtualMachineUtils.c functions made to work in C++
extern "C"{
	TVMMainEntry VMLoadModule(const char *module);
	TVMStatus VMFilePrint(int filedescriptor, const char *format, ...);
	TVMStatus VMThreadSleep(TVMTick tick);
	void MachineInitialize(int timeout);
	void MachineRequestAlarm(useconds_t usec, TMachineAlarmCallback callback, void *calldata);
	void MachineEnableSignals(void);

}
class tcb{
	public: 
	TVMThreadPriority priority; 
	TVMThreadID id; 
	TVMThreadState state;	
	TVMTick ticks;
	void *base; // stack pointer base
	void* params; 
	TVMMemorySize memsize;
	TVMMutexID mid;
	TVMThreadEntry entry; //entry function need to make a skeleton wrapper for it
	SMachineContext context; 
	
};
TVMThreadID current; // ptr to the current thread 
vector<tcb*> all;
//multiple ready queues one for each priority, thread state declares what goes first 
//goes into the ready queue when thread is activated
list<tcb*> high;
list<tcb*> normal;
list<tcb*> low;
//sleeping thread queue
vector<tcb*> sleeping;
//mutex thread queue


// The scheduler is responsible for determining what is the highest priority and therefore what should be running
int volatile gtick;

void idleFun(void*);
void Skeleton(void* params);
void Ready(TVMThreadID thread); //pushes into a queue based on priority
void AlarmCallback(void *params);
void scheduler(void);
//will go through queues and run the thread with highest priority

TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[])
{
	
	tcb primary;
	primary.id = all.size(); 
	primary.priority = VM_THREAD_PRIORITY_NORMAL;
	primary.state = VM_THREAD_STATE_RUNNING;
	all.push_back(&primary);
//	*current = primary;
	current = primary.id;		
//primary thread doesnt need a context 	
	tcb idle; 
	idle.id = all.size();		
	idle.priority = VM_THREAD_PRIORITY_LOW;
	idle.state = VM_THREAD_STATE_READY;
	idle.memsize = 70000;
        idle.base = new uint8_t[idle.memsize];
	idle.entry = idleFun;
        MachineContextCreate(&(idle.context), *(idle.entry), NULL, idle.base, idle.memsize); 		
	all.push_back(&idle);
	MachineInitialize(machinetickms);
	MachineRequestAlarm(machinetickms, AlarmCallback, NULL); //arguments? and alarmCallback being called?	
        MachineEnableSignals();


	TVMMainEntry vmmain; //creates a function pointer
	vmmain = VMLoadModule(argv[0]); //set function pointer to point to this function
	if(vmmain == NULL)//check if vmmain is pointing to the address of VMLoadModule 
 		cout<< "is  null!" << endl;
	else
		vmmain(argc, argv);//run the function it points to, in a way it derefences it

  	return(VM_STATUS_SUCCESS);
}

void idleFun(void*){ while(1){} }
void scheduler(){





}
void Ready(TVMThreadID thread){
	
	switch(all[thread]->priority){
		case VM_THREAD_PRIORITY_LOW: low.push_back(all[thread]); break;
		case VM_THREAD_PRIORITY_NORMAL: high.push_back(all[thread]); break;
		case VM_THREAD_PRIORITY_HIGH: normal.push_back(all[thread]); break;		
	} 
	return;


} //pushes into a queue based on priority

TVMStatus VMTerminate(TVMThreadID thread)
{  
	all[thread]->state = VM_THREAD_STATE_DEAD;
	//check through the ready queues 
	vector<tcb*>::iterator itr; 
	for(itr = high.begin(); itr != high.end(); itr++){
		if((*itr)->id == thread)
			high.remove((*itr));
	}
	
	return(VM_STATUS_SUCCESS);
}

void Skeleton(void* params){
	params = all[current]->params;
	MachineEnableSignals(); 
	all[current]->entry(params);
	VMTerminate(all[current]->id);
	
}  
void AlarmCallback(void *param){
	vector<tcb*>::iterator itr; 
	for(itr = sleeping.begin(); itr != sleeping.end(); itr++)
	{
		if((*itr)->ticks > 0)
			(*itr)->ticks--;
		else { 
			(*itr)->state = VM_THREAD_STATE_READY;
			Ready((*itr)->id); //put into a ready queue
		}
		
	}
}

TVMStatus VMThreadSleep(TVMTick tick){
	all[current]->ticks = tick;//possibly have to multiply by 1000
	all[current]->state = VM_THREAD_STATE_WAITING;
	sleeping.push_back(all[current]); //a function that looks through threads and adds them to the sleep queue
	//schelduler
	return(VM_STATUS_SUCCESS);
}


TVMStatus VMFileWrite(int filedescriptor, void *data, int *length){
	write(filedescriptor, data, *length);
	return(VM_STATUS_SUCCESS);
}
//check flow chart
TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid){
	TMachineSignalState oldstate;
	MachineSuspendSignals(&oldstate);
	tcb thread;
	*tid = all.size();
	thread.id = all.size();
	thread.entry  = entry;
	thread.params = param;
	thread.memsize = memsize;
	thread.priority = prio;
	thread.state = VM_THREAD_STATE_DEAD;
	thread.base = new uint8_t[thread.memsize];
		

		
	return VM_STATUS_SUCCESS;
	
}

TVMStatus VMThreadActivate(TVMThreadID thread){
	TMachineSignalState oldstate;
	MachineSuspendSignals(&oldstate);
	//MachineCreateContext(&(all[current], )
        MachineContextCreate(&(all[current]->context), *(all[current]->entry), NULL, all[current]->base, all[current]->memsize); 		
	all[thread]->state = VM_THREAD_STATE_READY;	
	Ready(all[thread]->id);//put into a ready queue 
	
	
	return VM_STATUS_SUCCESS;

}

TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref)
{
	if(!stateref)
		return VM_STATUS_ERROR_INVALID_PARAMETER;		

	vector<tcb*>::iterator itr; 	
	for(itr = all.begin(); itr != all.end(); itr++){
		if((*itr)->id == thread){
			*stateref = all[thread]->state;
			return(VM_STATUS_SUCCESS);
		}
	}

	return(VM_STATUS_SUCCESS);
}

