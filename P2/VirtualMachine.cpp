#include "VirtualMachine.h"
#include "Machine.h"
#include <iostream>
#include <unistd.h>
#include <vector>
#include <string>
using namespace std;

class ThreadControlBlock {
	public:
		TVMThreadID threadID;
		TVMThreadState threadState;
		TVMThreadPriority threadPriority;
		TVMMemorySize threadMemory;
		TVMTick ticksThreads;
		TVMThreadEntry threadEntryPos;
		SMachineContext infoThread;
		void* PtrforStack;
		void* parameterThread;
};

extern "C" {
	class ThreadControlBlock;
	const int timeout = 100000;
	TVMThreadID runningThreadID;
	TVMThreadID idleThreadID;
	volatile int global_tick;
	
	vector<ThreadControlBlock> totalThreads; //vector that holds all of the threads
	vector<TVMThreadID>	lpThreads; //vector that holds low priority threads
	vector<TVMThreadID> npThreads; //vector that holds normal priority threads
	vector<TVMThreadID> hpThreads; //vector that holds high priority threads
	vector<TVMThreadID> sleepThreads; //vector for sleep threads

	TVMMainEntry VMLoadModule(const char *module);
	TVMStatus VMThreadSleep(TVMTick tick);
	void entryThread(void* param);	
	void idleThreadFun(void* calldata);
	void threadsDequeue(TVMThreadID argThreadID);
	void threadsOnEnqueue(TVMThreadID argThreadID);
	void call_back(void* calldata);
	void scheduler(void);

	TVMStatus VMStart(int tickms, int machinetickms, int argc,char *argv[]) {
	
	// Create Main Thread & set it as runningThreadID
	MachineInitialize(machinetickms);
	MachineRequestAlarm(machinetickms, call_back, NULL); //arguments? and alarmCallback being called?	
	MachineEnableSignals();
	
	ThreadControlBlock primary_Thread; //initialized thread
	primary_Thread.threadID = totalThreads.size(); //
	primary_Thread.threadPriority = VM_THREAD_PRIORITY_NORMAL;
	runningThreadID = primary_Thread.threadID;
	primary_Thread.threadState = VM_THREAD_STATE_RUNNING;
	totalThreads.push_back(primary_Thread);	

	
	// Setup idle_Thread
	ThreadControlBlock idle_Thread;
	idle_Thread.threadID = totalThreads.size();
	idle_Thread.threadPriority = VM_THREAD_PRIORITY_LOW;
	idle_Thread.threadState = VM_THREAD_STATE_READY;
	idle_Thread.threadEntryPos = &idleThreadFun;
	idle_Thread.threadMemory = 64000;
	idle_Thread.PtrforStack = new uint8_t[idle_Thread.threadMemory];	
	
	// This creates the context for idle_Thread
	MachineContextCreate(&(idle_Thread.infoThread), &entryThread, 0, idle_Thread.PtrforStack, idle_Thread.threadMemory);
	
	totalThreads.push_back(idle_Thread);		
	idleThreadID = idle_Thread.threadID;
	// DO NOT ENQUEUE idle thread. Treat it as the ELSE thing to do in scheduler.

	
	// Set the fn ptr to the address returned from the VMLoadModule
	TVMMainEntry vmMainPtr = VMLoadModule((const char*) argv[0]);	
	
	// Make sure the address returned isn't an error
	if(!vmMainPtr) {
		return VM_STATUS_FAILURE;
	}
	
	// Initialize machine with timeout
	MachineInitialize(timeout);
		
	// Set pointer to callback function.
	TMachineAlarmCallback callbackPtr = &call_back;
		
	// Set MachineRequestAlarm
	MachineRequestAlarm(tickms*1000, callbackPtr, 0);
		
	// Enable Signals
	MachineEnableSignals();
		
	// Call the function pointed at by the VMLoadModule

	vmMainPtr (argc, argv);

	// Once you successfully return from whatever VMMain was pointer to
	// you can return, saying it was a success.
	return VM_STATUS_SUCCESS;
}

	TVMStatus VMFileWrite(int filedescriptor, void *data, int *length) {

	if (!data || !length)
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	
	int returnCode = write(filedescriptor, data, *length);
		
	if(returnCode < 0)
		return VM_STATUS_FAILURE;
	else 
		return VM_STATUS_SUCCESS;
}

	TVMStatus VMThreadSleep(TVMTick tick) {

	if(tick == VM_TIMEOUT_INFINITE) {
		return VM_STATUS_ERROR_INVALID_PARAMETER;	
	}
	
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	
	if(tick == VM_TIMEOUT_IMMEDIATE) {
		// 1. Mark running process as ready (currently its "running")
		totalThreads[runningThreadID].threadState = VM_THREAD_STATE_READY;
		
		// 2. Put it at the back of the readyqueue
		threadsOnEnqueue(runningThreadID);
		
		// 3. Schedule
		scheduler();
		
		// 4. Resume Signals
		MachineResumeSignals(&OldState);
		return VM_STATUS_SUCCESS;
	}	
	

	
	// 1. Set current thread (in runningThreads)'s state to waiting	
	totalThreads[runningThreadID].threadState = VM_THREAD_STATE_WAITING;
	// 2. Set current thread (in runningThreads)'s tick to the argTick
	totalThreads[runningThreadID].ticksThreads = tick;
	// 3. Add to sleeping vector
	sleepThreads.push_back(runningThreadID);
	// 4. scheduler() // probably to idle
	//cout << "Sleep is calling scheduler()" << endl;

	scheduler();	// running already set to WAITING
	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;	
}

	void call_back(void* calldata) { 
	std::vector<TVMThreadID>::const_iterator i;
	
	// iterate through the vector of sleeping threads
	for(i = sleepThreads.begin(); i != sleepThreads.end(); ++i) {
		
		// decrement each threads' tick by 1	
		(totalThreads[(*i)].ticksThreads)--;
		
		// if any of the vector's thread's tick hits zero
		if(totalThreads[(*i)].ticksThreads == 0) {
			//cout << "CallBack(): Hey! thread# " << (*i) << "just woke up!" << endl;
			
			// make it's state ready
			totalThreads[(*i)].threadState = VM_THREAD_STATE_READY;
				
			//push to ready queue
			threadsOnEnqueue(*i);			
			
			// Change running thread's state
			totalThreads[runningThreadID].threadState = VM_THREAD_STATE_READY;
			
			//Schedule
			scheduler();
			
			return;
		}
	}
}

	void scheduler() {

	// running thread's state should ALREADY BE MODIFIED BEFORE CALLING SCHEDULE
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	
	if(!hpThreads.empty()) {
		if(runningThreadID == hpThreads.front()) {
			// cout << "First high priority thread is already running." << endl;
			MachineResumeSignals(&OldState);
			return;
		}
		
		// Otherwise switch context.
		//cout << "Time to switch to a different high priority thread!" << endl;
		
		TVMThreadID temp = runningThreadID;
		runningThreadID = hpThreads.front();
		
		hpThreads.erase(hpThreads.begin());		// Taking it out of READY QUEUE only!
		
		MachineResumeSignals(&OldState);
		MachineContextSwitch(&totalThreads[temp].infoThread, &totalThreads[runningThreadID].infoThread);
		// DO NOTHING AFTER CONTEXT SWITCH ERROR. NO WAY EVER!
	}
	
	else if(!npThreads.empty()) {
		if(runningThreadID == npThreads.front()) {
		//	cout << "First normal priority thread is already running." << endl;
 			MachineResumeSignals(&OldState);
			return;
		}
		
		// Otherwise switch context.
		//cout << "Time to switch to a different normal priority thread!" << endl;
		
		TVMThreadID temp = runningThreadID;
		runningThreadID = npThreads.front();

		//cout << "\n Current thread id = " << temp << "\nTrying to switch to ID: " << runningThreadID << endl;
		
		npThreads.erase(npThreads.begin());		// Taking it out of READY QUEUE only!
		MachineResumeSignals(&OldState);
		MachineContextSwitch(&totalThreads[temp].infoThread, &totalThreads[runningThreadID].infoThread);
		// DO NOTHING AFTER CONTEXT SWITCH ERROR. NO WAY EVER!
	}
	
	else if (!lpThreads.empty()) {
		if(runningThreadID == lpThreads.front()) {
			//cout << "First low priority thread is already running." << endl;
 			MachineResumeSignals(&OldState);
			return;
		}
		
		// Otherwise switch context.
		//cout << "Time to switch to a different low priority thread!" << endl;
		
		TVMThreadID temp = runningThreadID;
		runningThreadID = lpThreads.front();
		
		lpThreads.erase(lpThreads.begin());		// Taking it out of READY QUEUE only!
		MachineResumeSignals(&OldState);
		MachineContextSwitch(&totalThreads[temp].infoThread, &totalThreads[runningThreadID].infoThread);
		// DO NOTHING AFTER CONTEXT SWITCH ERROR. NO WAY EVER!
	}
	
	else {
		// If all are empty, switch to idle thread.
	//	cout << "Time to switch to idle thread!" << endl;	
	
		TVMThreadID temp = runningThreadID;
		runningThreadID = idleThreadID;		
		
		MachineResumeSignals(&OldState);
		MachineContextSwitch(&totalThreads[temp].infoThread, &totalThreads[runningThreadID].infoThread);
	}	

}	// End of scheduler()
		
	void entryThread(void* param) {

	// 1. Enable Signals
	MachineEnableSignals();
		
	// 2. Call Entry
	void* myParam = totalThreads[runningThreadID].parameterThread;
	totalThreads[runningThreadID].threadEntryPos(myParam);
		
	// 3. Terminate Thread
	VMThreadTerminate(runningThreadID);
	return;
}
	
	TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid) {
	// Return error if  entry ot tid is NULL
	if(!entry || !tid)
	return VM_STATUS_ERROR_INVALID_PARAMETER;
	
	// 1. Suspend signals
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	
	// 2. Initiate Thread			
	ThreadControlBlock newThread;
	newThread.threadID = totalThreads.size();
	newThread.threadPriority = prio;
	newThread.threadState = VM_THREAD_STATE_DEAD;
	newThread.threadMemory = memsize;
	newThread.PtrforStack = new uint8_t[memsize];
	newThread.threadEntryPos = entry;
	newThread.parameterThread = param;
	
	// 3. Store thread object
	totalThreads.push_back(newThread);
	
	// 4. Store thread id in param's id
	*tid = newThread.threadID;
	
	// 6. Resume signals and return
	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
}

	TVMStatus VMThreadActivate(TVMThreadID thread) {

	int found = 0;
	vector<ThreadControlBlock>::iterator i;
	
	for(i = totalThreads.begin(); i != totalThreads.end(); ++i) {
		if(i->threadID == thread) {
			found = 1;
			break;
		}
	}
	if(!found)
		return VM_STATUS_ERROR_INVALID_ID;
	if(i->threadState != VM_THREAD_STATE_DEAD)
		return VM_STATUS_ERROR_INVALID_STATE;
	// Note: By now, iterator i should have the correct thread.

	// 1. Suspend signals
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	
	// 2. Create Context for the thread we are activating. 	// Good Example: MachineContextCreate(&(idle_Thread.infoThread), &entryThread, 0, idle_Thread.PtrforStack, idle_Thread.threadMemory);	
	MachineContextCreate(&(i->infoThread), &entryThread, i->parameterThread, i->PtrforStack, i->threadMemory);
	
	// 3. Set Thread State to Ready.
	i->threadState = VM_THREAD_STATE_READY;
	
	// 4. Push into queue according to priority.
	threadsOnEnqueue(i->threadID);
	
	// 5. Reschedule if needed.
	if(i->threadPriority > totalThreads[runningThreadID].threadPriority) {
		totalThreads[runningThreadID].threadPriority = VM_THREAD_STATE_READY;
		threadsOnEnqueue(totalThreads[runningThreadID].threadID);
		scheduler();
	}
	
	// 6. Resume signals.
	MachineResumeSignals(&OldState);
	
	return VM_STATUS_SUCCESS;
}

	TVMStatus VMThreadID(TVMThreadIDRef threadref) {

	if(!threadref)
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	
	vector<ThreadControlBlock>::iterator i;
	for(i = totalThreads.begin(); i != totalThreads.end(); ++i) {
		if(i->threadID == runningThreadID) {
			*threadref = runningThreadID;
			return VM_STATUS_SUCCESS;
		}
	}
	return VM_STATUS_ERROR_INVALID_ID;
}

	TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef state){

	if(!state)
		return VM_STATUS_ERROR_INVALID_PARAMETER;	
	
	vector<ThreadControlBlock>::iterator i;
	for(i = totalThreads.begin(); i != totalThreads.end(); ++i) {
		if(i->threadID == thread) {
			*state = i->threadState;
			return VM_STATUS_SUCCESS;
		}
	}
	return VM_STATUS_ERROR_INVALID_ID;
}

	TVMStatus VMThreadTerminate(TVMThreadID thread) {
	vector<ThreadControlBlock>::iterator i;
	int found = 0;
	for(i = totalThreads.begin(); i != totalThreads.end(); ++i) {
		if(i->threadID == thread) {
			found = 1;
			break;
		}
	}
	if(!found)
		return VM_STATUS_ERROR_INVALID_ID;
	if(i->threadState == VM_THREAD_STATE_DEAD)
		return VM_STATUS_ERROR_INVALID_STATE;
	
	// 1. Set state to dead.
	i->threadState = VM_THREAD_STATE_DEAD;
	
	// 2. Dequeue from all except totalThreads.
	threadsDequeue(i->threadID);
	
	// 3. Reschedule!!
	scheduler();
	
	return VM_STATUS_SUCCESS;
}

	void threadsDequeue(TVMThreadID argThreadID) {
	// This function weill remove the arg thread from 
	// sleep, and all priority queues.
	
	vector<TVMThreadID>::iterator i;
	int found = 0;
	
	// 1. Deal with sleepThreads.	
	for(i = sleepThreads.begin(); i != sleepThreads.end(); ++i) {
		if(*i == argThreadID)
			break;			
	}
	sleepThreads.erase(i);
	
	// 2. Deal with priority queues	
	TVMThreadPriority priority = totalThreads[argThreadID].threadPriority;	
	
	if(priority == VM_THREAD_PRIORITY_HIGH) {
		for(i = hpThreads.begin(); i != hpThreads.end(); ++i) {
			if(*i == argThreadID) {
				found = 1;
				break;		
			}				
		}
		if(found)
			hpThreads.erase(i);
	}
	else if (priority == VM_THREAD_PRIORITY_NORMAL){
		for(i = npThreads.begin(); i != npThreads.end(); ++i) {
			if(*i == argThreadID) {
				found = 1;
				break;			
			}
		}
		if(found) 
			npThreads.erase(i);		
	}
	else if (priority == VM_THREAD_PRIORITY_LOW) {
		for(i = lpThreads.begin(); i != lpThreads.end(); ++i) {
			if(*i == argThreadID) {
				found = 1;				
				break;
			}
		}
		if(found)
			lpThreads.erase(i);
	}
	else {
		cout << "threadsDequeue Error: Invalid Priority of passed thread." << endl;
	}	
	return;
}

	void idleThreadFun(void* calldata) {
	while(1){
		// do nothing
	}
	return;
}

	void threadsOnEnqueue(TVMThreadID argThreadID){

	TVMThreadPriority priority = totalThreads[argThreadID].threadPriority;
	
	if(priority == VM_THREAD_PRIORITY_LOW) {
		lpThreads.push_back(argThreadID);
	}
	else if (priority == VM_THREAD_PRIORITY_NORMAL) {
		npThreads.push_back(argThreadID);
	}
	else if (priority == VM_THREAD_PRIORITY_HIGH) {
		hpThreads.push_back(argThreadID);
	}
	else {
		cout << "Error: Invalid thread priority in threadsOnEnqueue()." << endl;
		return;
	}
	
	string prio;
	if(priority == 1)
		prio = "low";
	else if (priority == 2)
		prio = "normal";
	else if (priority == 3)
		prio = "high";
	else
		prio = "error";
	
	//cout << "Enqueue: Added thread  with id# " << argThreadID << 
	//" to " << prio << " ready queue." << endl;
	return;
	}
	
	
}

