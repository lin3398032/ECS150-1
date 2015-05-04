#include "VirtualMachine.h"
#include "Machine.h"
#include <unistd.h>
#include <vector>
#include <string>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/stat.h> 
#include <sys/wait.h>
# include <sys/mman.h>
#include <sys/file.h> 
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


typedef struct Mutex{
	
	bool lock; //the mutex lock
	TVMMutexID mid;
	vector<TVMTheradIDRef> waitku; //the waitku, uses pointers to the blocks
	
};



extern "C" {
	class ThreadControlBlock;
	const int timeout = 100000;
	TVMThreadID runningThreadID;
	TVMThreadID idleThreadID;
	volatile int gtick;
	
	vector<ThreadControlBlock> totalThreads; //vector that holds all of the threads
	vector<TVMThreadID>	lpThreads; //vector that holds low priority threads
	vector<TVMThreadID> npThreads; //vector that holds normal priority threads
	vector<TVMThreadID> hpThreads; //vector that holds high priority threads
	vector<TVMThreadID> sleepThreads; //vector for sleep threads

	TVMMainEntry VMLoadModule(const char *module);
	TVMStatus VMThreadSleep(TVMTick tick);
	void entryThread(void* param);	
	void idleThreadFun(void* calldata);
	void dequeueThread(TVMThreadID id);
	void enqueueThread(TVMThreadID id);
	void call_back(void* calldata);
	void scheduleThread(void);
}
TVMStatus VMStart(int tickms, int machinetickms, int argc,char *argv[]) {
	
	ThreadControlBlock primary_Thread; //initialized thread
	primary_Thread.threadID = totalThreads.size(); //
	primary_Thread.threadPriority = VM_THREAD_PRIORITY_NORMAL;
	runningThreadID = primary_Thread.threadID; //store id of the current thread
	primary_Thread.threadState = VM_THREAD_STATE_RUNNING;
	totalThreads.push_back(primary_Thread);	//push primary into totals

	
	// Setup idle_Thread
	ThreadControlBlock idle_Thread;
	idle_Thread.threadID = totalThreads.size(); //size changes so next ID number

	//set up for function pointer
	idle_Thread.threadEntryPos = &idleThreadFun; 
	idle_Thread.threadMemory = 70000;//allocate memory
	
	//seet priority 
	idle_Thread.threadPriority = VM_THREAD_PRIORITY_LOW;
	idle_Thread.threadState = VM_THREAD_STATE_READY;
	
	idle_Thread.PtrforStack = new uint8_t[idle_Thread.threadMemory];//declare pointer for stack	
	
	// This creates the context for idle_Thread
	MachineContextCreate(&(idle_Thread.infoThread), *entryThread, 0, idle_Thread.PtrforStack, idle_Thread.threadMemory);
	//void MachineContextCreate(SMachineContextRef mcntxref, void (*entry)(void *), void *param, void *stackaddr, size_t stacksize);
	totalThreads.push_back(idle_Thread); //now push into totalThreads		
	idleThreadID = idle_Thread.threadID;


	TVMMainEntry vmmain = VMLoadModule(argv[0]);	
	if(vmmain == NULL) 
		return VM_STATUS_FAILURE;

	// Initialize machine with timeout
	MachineInitialize(timeout);
	MachineRequestAlarm(tickms*1000, call_back, NULL); //arguments? and alarmCallback being called?	
	MachineEnableSignals();

	vmmain (argc, argv);
	return VM_STATUS_SUCCESS;
}

TVMStatus VMFileWrite(int fd, void *data, int *length) {

	if (!data || !length)
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	
	int ret = write(fd, data, *length);
		
	if(ret > 0)
		return VM_STATUS_SUCCESS;
	else 
		return VM_STATUS_FAILURE;
}

TVMStatus VMThreadSleep(TVMTick tick) {

	if(tick == VM_TIMEOUT_INFINITE)
		return VM_STATUS_ERROR_INVALID_PARAMETER;	
	
	TMachineSignalState old_state;
	MachineSuspendSignals(&old_state);//have to suspend all signals
	
	//timeout is immediate then then put back into ready and queue it
	if(tick == VM_TIMEOUT_IMMEDIATE) {
		totalThreads[runningThreadID].threadState = VM_THREAD_STATE_READY; //put running thread into the read state
		enqueueThread(runningThreadID); //queue it
		scheduleThread(); //call schelduler
		MachineResumeSignals(&old_state);
		return VM_STATUS_SUCCESS;
	}	
	

	//not infinite and not immediate then actually sleep
	totalThreads[runningThreadID].threadState = VM_THREAD_STATE_WAITING;
	totalThreads[runningThreadID].ticksThreads = tick; 
	sleepThreads.push_back(runningThreadID); //  into the sleeping threads

	scheduleThread();
	MachineResumeSignals(&old_state);
	return VM_STATUS_SUCCESS;	
}
//alarm callback 
void call_back(void* data) { 
	vector<TVMThreadID>::const_iterator itr;
	

	for(itr = sleepThreads.begin(); itr != sleepThreads.end(); ++itr) {
		
		(totalThreads[(*itr)].ticksThreads)--;
		if(totalThreads[(*itr)].ticksThreads == 0) {

			totalThreads[(*itr)].threadState = VM_THREAD_STATE_READY;
			enqueueThread(*itr);			
			totalThreads[runningThreadID].threadState = VM_THREAD_STATE_READY;
			scheduleThread();
			return;
		}
	}
}

void scheduleThread() {
	TMachineSignalState old_state;
	MachineSuspendSignals(&old_state);
	if (!lpThreads.empty()) {
		if(runningThreadID == lpThreads.front()) {
 			MachineResumeSignals(&old_state);
			return;
		}
		
		TVMThreadID temp = runningThreadID;
		runningThreadID = lpThreads.front();
		lpThreads.erase(lpThreads.begin());	
		MachineResumeSignals(&old_state);
		MachineContextSwitch(&totalThreads[temp].infoThread, &totalThreads[runningThreadID].infoThread);
	}
	
	else if(!hpThreads.empty()) {
		//check if its currently running
		if(runningThreadID == hpThreads.front()) {
			MachineResumeSignals(&old_state);
			return;
		}
		

		TVMThreadID temp = runningThreadID; 
		runningThreadID = hpThreads.front(); //run high priority thread
		hpThreads.erase(hpThreads.begin()); // delete from queue		
		MachineResumeSignals(&old_state);
		MachineContextSwitch(&totalThreads[temp].infoThread, &totalThreads[runningThreadID].infoThread); //switch the context
	}
	
	else if(!npThreads.empty()) {
		if(runningThreadID == npThreads.front()) {
 			MachineResumeSignals(&old_state);
			return;
		}
		
		TVMThreadID temp = runningThreadID;
		runningThreadID = npThreads.front();
		npThreads.erase(npThreads.begin());		
		MachineResumeSignals(&old_state);
		MachineContextSwitch(&totalThreads[temp].infoThread, &totalThreads[runningThreadID].infoThread);
	}

	//if all empty
	else {
		TVMThreadID temp = runningThreadID;
		runningThreadID = idleThreadID;		
		MachineResumeSignals(&old_state);
		MachineContextSwitch(&totalThreads[temp].infoThread, &totalThreads[runningThreadID].infoThread);
	}	

}
void entryThread(void* param) {

	MachineEnableSignals();
	void* myParam = totalThreads[runningThreadID].parameterThread;
	totalThreads[runningThreadID].threadEntryPos(myParam);
	VMThreadTerminate(runningThreadID);
	return;
}

void create_thread_block(){
}


TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize size, TVMThreadPriority priority, TVMThreadIDRef tid) {
	if(!entry || !tid)
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	

	TMachineSignalState old_state;
	MachineSuspendSignals(&old_state);
	//intialize the thread
	ThreadControlBlock new_t();
	new_t.threadID = totalThreads.size();
	new_t.threadPriority = priority;
	new_t.threadState = VM_THREAD_STATE_DEAD;
	new_t.threadMemory = size;
	new_t.PtrforStack = new uint8_t[memsize];
	new_t.threadEntryPos = entry;
	new_t.parameterThread = param;
	
	//push on to the total threads 
	totalThreads.push_back(new_t);

	*tid = new_t.threadID;//??
	
	MachineResumeSignals(&old_state);
	return VM_STATUS_SUCCESS;
}

TVMStatus VMThreadActivate(TVMThreadID id) {

	int found = 0;
	vector<ThreadControlBlock>::iterator itr;
	
	for(itr = totalThreads.begin(); itr != totalThreads.end(); ++itr) {
		if(itr->threadID == id) {
			found = 1;
			break;
		}
	}
	if(!found)
		return VM_STATUS_ERROR_INVALID_ID;
	if(itr->threadState != VM_THREAD_STATE_DEAD)
		return VM_STATUS_ERROR_INVALID_STATE;



	TMachineSignalState old_state;
	MachineSuspendSignals(&old_state);
	MachineContextCreate(&(itr->infoThread), &entryThread, itr->parameterThread, itr->PtrforStack, itr->threadMemory);
	itr->threadState = VM_THREAD_STATE_READY;
	enqueueThread(itr->threadID);
	
	if(itr->threadPriority > totalThreads[runningThreadID].threadPriority) {
		totalThreads[runningThreadID].threadPriority = VM_THREAD_STATE_READY;
		enqueueThread(totalThreads[runningThreadID].threadID);
		scheduleThread();
	}
	
	MachineResumeSignals(&old_state);
	
	return VM_STATUS_SUCCESS;
}

TVMStatus VMThreadID(TVMThreadIDRef reference) {

	if(!reference)
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	
	vector<ThreadControlBlock>::iterator itr;
	for(itr = totalThreads.begin(); itr != totalThreads.end(); ++itr) {
		if(itr->threadID == runningThreadID) {
			*reference = runningThreadID;
			return VM_STATUS_SUCCESS;
		}
	}
	return VM_STATUS_ERROR_INVALID_ID;
}

TVMStatus VMThreadState(TVMThreadID t, TVMThreadStateRef s){

	if(!s)
		return VM_STATUS_ERROR_INVALID_PARAMETER;	
	
	vector<ThreadControlBlock>::iterator itr;
	for(itr = totalThreads.begin(); itr != totalThreads.end(); ++itr) {
		if(itr->threadID == t) {
			*s = itr->threadState;
			return VM_STATUS_SUCCESS;
		}
	}
	return VM_STATUS_ERROR_INVALID_ID;
}

TVMStatus VMThreadTerminate(TVMThreadID thread) {
	vector<ThreadControlBlock>::iterator itr;
	int found = 0;
	for(itr = totalThreads.begin(); itr != totalThreads.end(); ++itr) {
		if(itr->threadID == thread) {
			found = 1;
			break;
		}
	}
	if(!found)
		return VM_STATUS_ERROR_INVALID_ID;
	if(itr->threadState == VM_THREAD_STATE_DEAD)
		return VM_STATUS_ERROR_INVALID_STATE;
	

	itr->threadState = VM_THREAD_STATE_DEAD;
	dequeueThread(itr->threadID);
	scheduleThread();
	
	return VM_STATUS_SUCCESS;
}

void dequeueThread(TVMThreadID id) {

	vector<TVMThreadID>::iterator itr;
	int found = 0;
	
	for(itr = sleepThreads.begin(); itr != sleepThreads.end(); ++itr) {
		if(*itr == id)
			break;			
	}
	sleepThreads.erase(itr);
	
	TVMThreadPriority priority = totalThreads[id].threadPriority;	
	
	if(priority == VM_THREAD_PRIORITY_HIGH) {
		for(itr = hpThreads.begin(); itr != hpThreads.end(); ++itr) {
			if(*itr == id) {
				found = 1;
				break;		
			}				
		}
		if(found)
			hpThreads.erase(itr);
	}
	else if (priority == VM_THREAD_PRIORITY_NORMAL){
		for(itr = npThreads.begin(); itr != npThreads.end(); ++itr) {
			if(*itr == id) {
				found = 1;
				break;			
			}
		}
		if(found) 
			npThreads.erase(itr);		
	}
	else if (priority == VM_THREAD_PRIORITY_LOW) {
		for(itr = lpThreads.begin(); itr != lpThreads.end(); ++itr) {
			if(*itr == id) {
				found = 1;				
				break;
			}
		}
		if(found)
			lpThreads.erase(itr);
	}
	return;
}

void idleThreadFun(void* calldata) {
	while(true){ 
	//nothing 
	}
	return;
}

void enqueueThread(TVMThreadID id){


	if (totalThreads[id].threadPriority == VM_THREAD_PRIORITY_HIGH) 
		hpThreads.push_back(id);
	else if (totalThreads[id].threadPriority == VM_THREAD_PRIORITY_NORMAL) 
		npThreads.push_back(id);
	else if(totalThreads[id].threadPriority == VM_THREAD_PRIORITY_LOW) 
		lpThreads.push_back(id);
	else 
	   return;
	
	
	string p;
	if(totalThreads[id].threadPriority == 3)
		p = "HIGH";
	else if(totalThreads[id].threadPriority == 2)
		p = "NORMAL";
	else if(totalThreads[id].threadPriority == 1)
		p = "LOW";
	else
		p = "ERROR";
	return;
}
TVMStatus VMMutexCreate(TVMMutexIDRef mutexref){
	//check if reference to resource exsists in resources
	//if not create new mutex wi
	Mutex new_m;
	int semaphore = shm_open( new_m, O_CREAT | O_EXCL | O_RDWR , S_IRUSR | S_IWUSR );
	const size_t region_size = sysconf(_SC_PAGE_SIZE); //set to system's page size 
	return VM_STATUS_SUCCESS;
}

/*
	//semaphore
typedef struct {
	int value;
	char** list;
} Semaphore;

void wait(Semaphore *s){
		s->value--;
		while(s->value < 0){
			//add process to wait list 
			//block the process
			
		}
	
	
}


int main(int argc, char *argv[]){


	Semaphore S;
	const size_t region_size = sysconf(_SC_PAGE_SIZE); //set to system's page size 
	string str = "Kyle";
	const char *name = str.c_str();	
	str = "sema";
	const char *sema = str.c_str();	
	int s = shm_open( sema, O_CREAT | O_EXCL | O_RDWR , S_IRUSR | S_IWUSR );

	int i = shm_open( name, O_CREAT | O_EXCL | O_RDWR , S_IRUSR | S_IWUSR );
	if(i < 0){
		
		cerr << i << " shared memory failed\n" << endl;
		exit(0);
	}
	
	int c = ftruncate(i, region_size);
	if(c != 0){
	
		perror("ftruncate");
		exit(0);
		
	}
	void *ptr = mmap(0, region_size, PROT_READ | PROT_WRITE, MAP_SHARED, i, 0);
	if(ptr == MAP_FAILED){
		perror("mmap");
		close(i);
		exit(0);
	}
	
	pid_t pid = fork();
	if(pid == 0){ // child
		strcpy( (char *)ptr, "hello from your child!\n\n");
		exit(0);
	}
	if(pid > 0){ //parent
	
		int status;
		waitpid(pid, &status, 0);
		printf("%s", (char*)ptr);
	
	}
	
	int r = munmap(ptr, region_size);
	if(r != 0){
		perror("munmap");
		exit(0);
	}
	
	
	r = shm_unlink(name);
	if(r != 0){
		perror("shm_unlink");
		exit(0);
	}
		

	
	
	
	
	return(0);
}


*/


