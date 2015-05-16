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
	//cout << "current is  " << current << endl; 	
//primary thread doesnt need a context 
	tcb* tidle = new tcb;	
	tidle->id = all.size();		
	tidle->priority = VM_THREAD_PRIORITY_LOW;
	tidle->state = VM_THREAD_STATE_READY;
	tidle->memsize = 1000000;
        tidle->base = new uint8_t[tidle->memsize];
	tidle->entry = idleFun;
        MachineContextCreate(&(tidle->context), *Skeleton, NULL, tidle->base, tidle->memsize); 		
	all[tidle->id] = (tidle);
	idle = tidle->id;
	//cout << "idle is " << idle << endl; 
	MachineInitialize(machinetickms);
	MachineRequestAlarm(machinetickms*1000, AlarmCallback, NULL); //arguments? and alarmCallback being called?	
        MachineEnableSignals();


	TVMMainEntry vmmain; //creates a function pointer
	vmmain = VMLoadModule(argv[0]); //set function pointer to point to this function
	if(vmmain == NULL)//check if vmmain is pointing to the address of VMLoadModule 
 		cout<< "is  null!" << endl;
	else
		vmmain(argc, argv);//run the function it points to, in a way it derefences it

  	return(VM_STATUS_SUCCESS);
}

void idleFun(void*){  while(1){} }
void schedule(){
	//cout << "scheduler" << endl;
	//cout << "current thread id: " << current << endl; 	
	TMachineSignalState oldstate;
	MachineSuspendSignals(&oldstate);
	if(!high.empty() && high.front()->id != current && (high.front())->state != VM_THREAD_STATE_WAITING && (high.front())->state != VM_THREAD_STATE_DEAD){
			//cout << "context switched to high!" << endl;
			(high.front())->state = VM_THREAD_STATE_RUNNING;
			TVMThreadID tmp =(high.front())->id;
			TVMThreadID prev = current;
			current = tmp;	
			MachineContextSwitch(&all[prev]->context, &all[current]->context);

	}
	

	else if(!normal.empty() && normal.front()->id != current && (normal.front())->state != VM_THREAD_STATE_WAITING && (normal.front())->state != VM_THREAD_STATE_DEAD){
			if((normal.front())->state == VM_THREAD_STATE_DEAD){ cout << "its dead " << (normal.front())->id << endl; }
			(normal.front())->state = VM_THREAD_STATE_RUNNING;
			//cout << current <<" context switched to normal! " << (normal.front())->id << endl;
			TVMThreadID tmp =(normal.front())->id;
			TVMThreadID prev = current;
			current = tmp;
			MachineContextSwitch(&all[prev]->context, &all[current]->context);
		
	}

	else if(!low.empty() && low.front()->id != current && (low.front())->state != VM_THREAD_STATE_WAITING && (low.front())->state != VM_THREAD_STATE_DEAD){
			//cout << "context switched to low!" << endl;
			(low.front())->state = VM_THREAD_STATE_RUNNING;
			TVMThreadID tmp =(low.front())->id;
			TVMThreadID prev = current;
			current = tmp; 
			MachineContextSwitch(&all[prev]->context, &all[current]->context);
	
	}
	else{

	//	cout << "no threads found need to switch to idle!" << endl;
		TVMThreadID prev = current;
		current = idle; 
		MachineContextSwitch(&all[prev]->context, &all[current]->context);
	//	cout << "context switched" << endl;
	}

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
	return;	


} //pushes into a queue based on priority

TVMStatus VMTerminate(TVMThreadID thread)
{ 
	TMachineSignalState oldstate;
	MachineSuspendSignals(&oldstate);
	//all[thread]->state = VM_THREAD_STATE_DEAD;
	//remove through the ready queues 
	list<tcb*>::iterator itr;
	if(!high.empty()){  
	for(itr = high.begin(); itr != high.end(); ++itr){
		if((*itr)->id == thread && (*itr)->state != VM_THREAD_STATE_DEAD){
			cout << "going to remove " << (*itr)->id << endl;
			(*itr)->state = VM_THREAD_STATE_DEAD;
			high.remove((*itr));
			cout << " removed " << endl;
			break;
	 		
		}
	}}
	if(!normal.empty()){
	for(itr = normal.begin(); itr != normal.end(); ++itr){
		if((*itr)->id == thread && (*itr)->state != VM_THREAD_STATE_DEAD){
			cout << "going to remove " << (*itr)->id << endl;
			(*itr)->state = VM_THREAD_STATE_DEAD;
			normal.remove((*itr));
			cout << " removed " << endl;
 			break;
		}
	}}
	if(!low.empty()){
	for(itr = low.begin(); itr != low.end(); ++itr){
		if((*itr)->id == thread && (*itr)->state != VM_THREAD_STATE_DEAD){
			cout << "going to remove " << (*itr)->id << endl;
			(*itr)->state = VM_THREAD_STATE_DEAD;
			low.remove((*itr));
			cout << " removed " << endl;
			break;
		}
	}}
//	cout << " terminated " << endl; 
	all[thread]->state = VM_THREAD_STATE_DEAD;
	schedule();
	VMThreadDelete(thread);
	//need one for low and normal and error checking according to specs
	 MachineResumeSignals(&oldstate);
	

	if(all[thread]->id == NULL) {return VM_STATUS_ERROR_INVALID_ID;}
	else if(all[thread]->state != VM_THREAD_STATE_DEAD){return VM_STATUS_ERROR_INVALID_STATE;}
	else{return VM_STATUS_SUCCESS;}
}
TVMStatus VMThreadDelete(TVMThreadID thread){
	map<TVMThreadID, tcb*>::iterator itr;
	for(itr = all.begin(); itr != all.end(); itr++){
		if(all[itr->first]->state != VM_THREAD_STATE_DEAD){
			cout << "Delete " << itr->first << endl;
			all.erase(itr);
			}
	}
	
	return(VM_STATUS_SUCCESS);

}
void Skeleton(void* params){
	MachineEnableSignals(); 
	all[current]->entry(all[current]->params);
	VMTerminate(all[current]->id);
	cout << "end of skeleton" << endl;
}  
void AlarmCallback(void *param){
	vector<tcb*>::iterator itr;
	itr = sleeping.begin(); 
	while(itr != sleeping.end())
	{	
	//	cout << "ticks left " << (*itr)->ticks << endl; 
		(*itr)->ticks--;
		if((*itr)->ticks == 0) { 
	//		cout << "tock" << endl;
			(*itr)->state = VM_THREAD_STATE_READY;
			Ready((*itr)->id); //put into a ready queue
			sleeping.erase(itr);
		} else { itr++; } 
		
	}
	schedule();
}

TVMStatus VMThreadSleep(TVMTick tick){
	//TMachineSignalState oldstate;
	//MachineSuspendSignals(&oldstate);
	all[current]->ticks = tick;//possibly have to multiply by 1000
	all[current]->state = VM_THREAD_STATE_WAITING;
	sleeping.push_back(all[current]); //a function that looks through threads and adds them to the sleep queue
	schedule();//cout << "put thread  " << current << " to sleep with " << all[current]->ticks << " ticks"<< endl;
	//MachineResumeSignals(&oldstate);
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
	//MachineContextCreate(&(thread->context), *Skeleton, thread->params, thread->base, thread->memsize); 		
	all[thread->id] = thread;//added to map 	
	//cout << "memsize from app " << memsize << endl;
	//cout << "check map: " << " prio " << all[*tid]->priority << " memsize " << all[*tid]->memsize << endl;
	//cout << "thread created "<< all[*tid]->id << " returning tid " << *tid  << " from  "  <<  thread->id  << " with thread state " << all[*tid]->state << endl;
	//need to create context
	
  	MachineResumeSignals(&oldstate);
	return VM_STATUS_SUCCESS;
	
}

TVMStatus VMThreadActivate(TVMThreadID thread){
	//need to handle high priority 
	//need to check and see if its dead first
	//cout << "Activate thread " << thread << endl;
	TMachineSignalState oldstate;
	MachineSuspendSignals(&oldstate); 
        MachineContextCreate(&(all[thread]->context), *Skeleton, all[thread]->params, all[thread]->base, all[thread]->memsize); 		
	all[thread]->state = VM_THREAD_STATE_READY;
	//cout << "activated thread: " << thread << " with a state of " << all[thread]->state << endl;   
	Ready(thread);//put into a ready queue 
	//schedule(); 	
	MachineResumeSignals(&oldstate);
	if(all[thread]->id == NULL) {return VM_STATUS_ERROR_INVALID_ID;}
	else if(all[thread]->state != VM_THREAD_STATE_DEAD){return VM_STATUS_ERROR_INVALID_STATE;}
	else{return VM_STATUS_SUCCESS;}
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

	if(all[thread]->id == NULL){return VM_STATUS_ERROR_INVALID_ID;}
	else if(stateref == NULL){return VM_STATUS_ERROR_INVALID_PARAMETER;}
	else {return VM_STATUS_SUCCESS;}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void MachineCallBack(void *calldata, int result){
	Ready(calldata)

}

//number of bytes is length
//@ location specificed by data
//to file specified by fd
//number of bytes returned will be placed into results when callback fct is called
//calldata will be passed into callback fct
/*
1. do all necessary checking specified in the PDF (the returns except for success and failure)
2. SuspendSignal
3. Call MachineFileWrite
4. set current thread state to waiting
5. call schedule
6. xxxxxx
7. xxxxxxxx
void MachineFileWrite(int fd, void *data, int length, TMachineFileCallback callback, void *calldata){
*/
TVMStatus VMFileWrite(int filedescriptor, void *data, int *length){
	TMachineSignalState oldstate;

	if(data == NULL || length == NULL){
		return VM_STATUS_ERROR_INVALID_PARAMETER;	
	}	
	else if(length == -1){
		return VM_STATUS_FAILURE;	
	}
	else{
		MachineSuspendSignals(&oldstate);
		MachineFileWrite(filedescriptor, *data, *length, MachineCallBack, current); 
		all[current]->state = VM_THREAD_STATE_WAITING
		schedule();

	}
	
	

}

