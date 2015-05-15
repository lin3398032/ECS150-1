#include "VirtualMachine.h"
#include "Machine.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <vector>
#include<list>
#include <map>
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
TVMThreadID idle; //idle thread 
map<TVMThreadID, tcb*> all;
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
void schedule(void);
//will go through queues and run the thread with highest priority

TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[])
{
	
	tcb* primary = new tcb;
	primary->id = all.size(); 
	primary->priority = VM_THREAD_PRIORITY_NORMAL;
	primary->state = VM_THREAD_STATE_RUNNING;
	all[primary->id] = (primary);
	current = primary->id;	
	cout << "current is  " << current << endl; 	
//primary thread doesnt need a context 
	tcb* tidle = new tcb;	
	tidle->id = all.size();		
	tidle->priority = VM_THREAD_PRIORITY_LOW;
	tidle->state = VM_THREAD_STATE_READY;
	tidle->memsize = 70000;
        tidle->base = new uint8_t[tidle->memsize];
	tidle->entry = idleFun;
        MachineContextCreate(&(tidle->context), *Skeleton, NULL, tidle->base, tidle->memsize); 		
	all[tidle->id] = (tidle);
	idle = tidle->id;
	cout << "idle is " << idle << endl; 
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

void idleFun(void*){  while(1){  cout << "idle" << endl; } }
void schedule(){
	cout << "scheduler" << endl;
	cout << "current thread id: " << current << endl; 	
	TMachineSignalState oldstate;
	MachineSuspendSignals(&oldstate);
	if(!high.empty()){
		if(current == (high.front())->id){
			cout << "current already schelduled" << endl;	
			MachineResumeSignals(&oldstate); 
			return;
	 	} else {
			cout << "context switched to high!" << endl;
			tcb tmp =(high.front())->id;
			TVMThreadID prev = current;
			current = tmp; 
			MachineContextSwitch(&all[prev]->context, &all[current]->context);
		}
	}

	else if(!normal.empty()){
		if(current == (normal.front())->id){
			cout << "current already schelduled" << endl;	
			MachineResumeSignals(&oldstate); 
			return;
	 	} else {
			cout << "context switched to normal!" << endl;
			tcb tmp =(normal.front())->id;
			TVMThreadID prev = current;
			current = tmp; 
			MachineContextSwitch(&all[prev]->context, &all[current]->context);
		}
	}

	else if(!low.empty()){
		if(current == (low.front())->id){
			cout << "current already schelduled" << endl;	
			MachineResumeSignals(&oldstate); 
			return;
	 	} else {
			cout << "context switched to low!" << endl;
			tcb tmp =(low.front())->id;
			TVMThreadID prev = current;
			current = tmp; 
			MachineContextSwitch(&all[prev]->context, &all[current]->context);
		}
	}
	else{

		cout << "no threads found need to switch to idle!" << endl;
		TVMThreadID prev = current;
		current = idle; 
		MachineContextSwitch(&all[prev]->context, &all[current]->context);
		cout << "context switched" << endl;
	}

	
	//if(!normal.empty())
	//if(!low.empty())
	MachineResumeSignals(&oldstate);
	return;

}
void Ready(TVMThreadID thread){
	cout << "ready the thread: " << thread << " with thread priority " << all[thread]->priority << endl;	
	switch(all[thread]->priority){
		case VM_THREAD_PRIORITY_LOW: low.push_back(all[thread]); return;
		case VM_THREAD_PRIORITY_NORMAL: normal.push_back(all[thread]); return;
		case VM_THREAD_PRIORITY_HIGH: high.push_back(all[thread]); return;		
	} 
	


} //pushes into a queue based on priority

TVMStatus VMTerminate(TVMThreadID thread)
{  
	cout << "thread termination" << endl; 
	TMachineSignalState oldstate;
	MachineSuspendSignals(&oldstate);
	all[thread]->state = VM_THREAD_STATE_DEAD;
	//check through the ready queues 
	list<tcb*>::iterator itr; 
	for(itr = high.begin(); itr != high.end(); itr++){
		if((*itr)->id == thread)
			high.remove((*itr));
	}
	for(itr = normal.begin(); itr != normal.end(); itr++){
		if((*itr)->id == thread)
			normal.remove((*itr));
	}
	for(itr = low.begin(); itr != low.end(); itr++){
		if((*itr)->id == thread)
			low.remove((*itr));
	}
	schedule(); 
	//need one for low and normal and error checking according to specs
	 MachineResumeSignals(&oldstate);
	return(VM_STATUS_SUCCESS);
}
TVMStatus VMThreadDelete(TVMThreadID thread){

	return(VM_STATUS_SUCCESS);

}
void Skeleton(void* params){
	params = all[current]->params;
	MachineEnableSignals(); 
	all[current]->entry(params);
	VMTerminate(all[current]->id);
	return;	
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
			schedule();
		}
		
	}
}

TVMStatus VMThreadSleep(TVMTick tick){
	TMachineSignalState oldstate;
	MachineSuspendSignals(&oldstate);
	all[current]->ticks = tick;//possibly have to multiply by 1000
	all[current]->state = VM_THREAD_STATE_WAITING;
	sleeping.push_back(all[current]); //a function that looks through threads and adds them to the sleep queue
	cout << "put thread  " << current << " to sleep with " << tick << " ticks"<< endl;
	schedule();
	MachineResumeSignals(&oldstate);
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
	tcb *thread = new tcb;
	*tid = all.size();
	thread->id = all.size();
	thread->entry  = entry;
	thread->params = param;
	thread->memsize = memsize;
	thread->priority = prio;
	thread->state = VM_THREAD_STATE_DEAD;
	thread->base = new uint8_t[thread->memsize];
	MachineContextCreate(&(thread->context), *Skeleton, thread->params, thread->base, thread->memsize); 		
	all[thread->id] = thread;//added to map 	
	cout << "memsize from app " << memsize << endl;
	cout << "check map: " << " prio " << all[*tid]->priority << " memsize " << all[*tid]->memsize << endl;
	cout << "thread created "<< all[*tid]->id << " returning tid " << *tid  << " from  "  <<  thread->id  << " with thread state " << all[*tid]->state << endl;
	//need to create context
	
  	MachineResumeSignals(&oldstate);
	return VM_STATUS_SUCCESS;
	
}

TVMStatus VMThreadActivate(TVMThreadID thread){
	cout << "Activate thread " << thread << endl;
	TMachineSignalState oldstate;
	MachineSuspendSignals(&oldstate); 
        MachineContextCreate(&(all[thread]->context), *Skeleton, NULL, all[thread]->base, all[thread]->memsize); 		
	all[thread]->state = VM_THREAD_STATE_READY;
	cout << "activated thread: " << thread << " with a state of " << all[thread]->state << endl;   
	Ready(thread);//put into a ready queue 
	schedule(); 	
	MachineResumeSignals(&oldstate);
	return VM_STATUS_SUCCESS;

}

TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref)
{
	if(!stateref)
		return VM_STATUS_ERROR_INVALID_PARAMETER;		

	map<TVMThreadID, tcb*>::iterator itr; 	
	for(itr = all.begin(); itr != all.end(); itr++){
		if(itr->first == thread){
			*stateref = all[thread]->state;
			return(VM_STATUS_SUCCESS);
		}
	}

	return(VM_STATUS_SUCCESS);
}

