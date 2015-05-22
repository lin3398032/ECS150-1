#include "VirtualMachine.h"
#include "Machine.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <vector>
#include<list>
#include <cstring>
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
	void *MachineInitialize(int timeout, size_t sharedsize);
	void MachineRequestAlarm(useconds_t usec, TMachineAlarmCallback callback, void *calldata);
	void MachineEnableSignals(void);
	const TVMMemoryPoolID VM_MEMORY_POOL_ID_SYSTEM = 0;
}
const TVMMemoryPoolID sharedID = 1; 
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
	int storeResult;
	
};
//this will be attributes of the memory pool
class memBlock{
	public:
		uint8_t *base; 
		TVMMemorySize size;
		bool free; 
};
class memPool {
	public:
	uint8_t *base; 
	TVMMemorySize size;
	TVMMemoryPoolID id; 
	list<memBlock> space;
	memPool(uint8_t* addbase,TVMMemorySize msize); 	
};
memPool::memPool(uint8_t* addbase,TVMMemorySize  msize){
	base = addbase;
	size = msize;
	memBlock init; 
	init.base = addbase;
	cout << "memPool being made with \t" << msize << "size of available memory" << endl;  
	init.size = msize; 
	init.free = true;  
	space.push_back(init); //push back first space of freed memory 
	cout << "Memory Pool Created with " << space.size() << "memBlocks" << endl;
}
//create list for allocated space and free space
map<TVMMemoryPoolID, memPool*> allMem;
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

TVMMemorySize heapsize; //how large the system memory pool is
//base is just pointer to system memory pool




// The scheduler is responsible for determining what is the highest priority and therefore what should be running
int volatile gtick;

void idleFun(void*);
void Skeleton(void* params);
void Ready(TVMThreadID thread); //pushes into a queue based on priority
void AlarmCallback(void *params);
void schedule(void);
//will go through queues and run the thread with highest priority

TVMStatus VMStart(int tickms, TVMMemorySize heapsize, int machinetickms, TVMMemorySize sharedsize, int argc, char *argv[])
{
	

	//TVMMemorySize sharedsize;
	//TVMMemoryPoolIDRef memory;
	uint8_t* base = new uint8_t[heapsize]; //creating pointer to the system memory pool
	memPool *sysMem = new memPool(base, heapsize);
	sysMem->id = VM_MEMORY_POOL_ID_SYSTEM;
	allMem[VM_MEMORY_POOL_ID_SYSTEM] = sysMem;
	cout << "number of memBlocks in sysMem " << sysMem->space.size() << endl;
	 //create and push main system memory?
	//cout << "Creating memory system pool with id:\t" << allMem.size() << endl;  
	//VMMemoryPoolCreate(base, heapsize, memory);   //creating the system memory pool
	//cout << "total mempools\t" << allMem.size() << endl;
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
        MachineContextCreate(&(tidle->context), Skeleton, NULL, tidle->base, tidle->memsize); //function is pointer		
	all[tidle->id] = (tidle);
	idle = tidle->id;
	//cout << "idle is " << idle << endl;
	sharedsize = ((sharedsize + 4095)/4096)*4096; 
	void* sharedAdd = MachineInitialize(machinetickms, sharedsize);
	memPool *sMem = new memPool((uint8_t*)sharedAdd, sharedsize);
	sMem->id = sharedID;
	allMem[sharedID] = sMem;
	cout << "number of memBlocks in sysMem " << sysMem->space.size() << endl;
	//VMMemoryPoolCreate(&sharedAdd, sharedsize, sharedID);
	MachineRequestAlarm(machinetickms*1000, AlarmCallback, NULL); //arguments? and alarmCallback being called?	
        MachineEnableSignals();


	TVMMainEntry vmmain; //creates a function pointer
	vmmain = VMLoadModule(argv[0]); //set function pointer to point to this function
	if(vmmain == NULL){//check if vmmain is pointing to the address of VMLoadModule 
 		return VM_STATUS_FAILURE;
	}
	else {
		vmmain(argc, argv);//run the function it points to, in a way it derefences it
		return(VM_STATUS_SUCCESS);
	}
}

TVMStatus VMMemoryPoolDeallocate(TVMMemoryPoolID memory, void *pointer){
	//pointer is the base of the memory block
	//if allocated then deallocate
	list<memBlock>::iterator itr = allMem[memory]->space.begin();
	for( ; itr != allMem[memory]->space.end(); ){
		list<memBlock>::iterator prev = itr--;
		list<memBlock>::iterator next = itr++;  
		//found allocated space 
		if(itr->base == (uint8_t*)pointer && itr->free == false){
			if(prev->free == true){
			//merge
				TVMMemorySize buffer = itr->size; 
				prev->size += buffer; //give it the allocated size  
				allMem[memory]->space.erase(itr); //erase the block of memory 
				return VM_STATUS_SUCCESS;
							 
							
			}//not check if block next to it is free 
			if(next->free == true)					
			{
			//merge
				TVMMemorySize buffer = itr->size; 
				next->size += buffer; //give it the allocated size  
				allMem[memory]->space.erase(itr); //erase the block of memory 
				return VM_STATUS_SUCCESS;
			
			} else {
				
				itr++;	
				}
	 	}

	}  

} 
TVMStatus VMMemoryPoolQuery(TVMMemoryPoolID memory, TVMMemorySizeRef bytesleft){
	
	TVMMemorySize bytes = 0; 	
	list<memBlock>::iterator itr; 
	itr = allMem[memory]->space.begin(); 
	while((itr != allMem[memory]->space.end())){
	
		if(itr->free == true)	
		 	bytes +=  itr->size;
 	  	itr++;	

	}
	*bytesleft = bytes; 

	
	return VM_STATUS_SUCCESS;

}
//first step to initialize memory pool
//allocated_size = (size + 63) & (~63);

//TVMMemoryPoolID => essentially an int 
//need to keep track of free memory(the size) and its base as well as allocated memory(the size) and its base 
//need to keep updating base as you allocated space (i.e. shift base by doing base += allocatedSize)
//that means that the size of the free memory is heapsize - allocatedSize 
//if a free space opens up between two filled spaces (i.e. |Allocated|Free(1)|Allocated|Free|Free|Free|) need to store Free(1) 
//if |Allocated|Free(1)|Free(2)|Allocated|Free|Free| need to merge Free(1) and Free(2) to make it one big free
//all should be done in VMMemoryPoolAllocate!
TVMStatus VMMemoryPoolAllocate(TVMMemoryPoolID memory, TVMMemorySize size, void **pointer){
	unsigned int allocated = (size + 0x3F) & (~0x3F); //rounds up to the next 64 bytes
	//if memory not in allMem
	cout << "memory id:\t" << memory << endl;
	cout << "memory size \t" << size << endl; 
	cout << "size of allMem should be 1 for thread.so, actual size: \t" <<  allMem.size() << endl;
	cout << "allMem[memory]->space.size()" << allMem[memory]->id << endl;
	if(memory > allMem.size()){
		cout << "allocation fail memory id invalid" << endl;
		return(VM_STATUS_ERROR_INVALID_PARAMETER);
	}
	if( pointer == NULL || size == 0 || memory < 0){ //pointer should be an address that we will init doesnt matter if NULL i believe 
		cout << "allocation fail one of the parameters is invalid" << endl;
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}
	list<memBlock>::iterator itr;
	cout << "allocating memBlock" << endl;
	for(itr = allMem[memory]->space.begin(); itr != allMem[memory]->space.end(); itr++ ){
		cout << "in for loop" << endl;	
		if(itr->free == true){
			cout << "free block found" << endl;
			cout << "space in block\t" << itr->size << endl;
			cout << "size to be allocated\t" << allocated << endl;
			//if free block is larger than allocated
			if(itr->size > allocated){
				uint8_t* oldbase = itr->base;
				itr->size = itr->size - allocated;
				itr->base = itr->base + allocated;
				memBlock *m = new memBlock;
				m->free = false; 
				m->size = allocated; 
				m->base = oldbase; 
				allMem[memory]->space.push_back(*m);    
				*pointer = m->base; //assigned allocate size to pointer
				cout << "base \t" << *pointer << endl;
				cout << "!!!!!!!!!!!!!!!memory allocated!!!!!!!!!!!!!!!!!!" << endl;
				return VM_STATUS_SUCCESS;
			} else {
				//should be checking for other free allocated blocks if any
				cout << "memory was not allocated" << endl;
				return(VM_STATUS_ERROR_INSUFFICIENT_RESOURCES);
			}
 

		}

	}
}

TVMStatus VMMemoryPoolCreate(void *base, TVMMemorySize size, TVMMemoryPoolIDRef memory){
	if(!high.empty() && high.front()->id != current && (high.front())->state != VM_THREAD_STATE_WAITING && (high.front())->state != VM_THREAD_STATE_DEAD){
	if(base == NULL || size == 0){
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}
	cout << "creating pool with ID " << allMem.size() << endl;
	memPool *mem = new memPool((uint8_t*)base, size);
	*memory = allMem.size();
	mem->id = allMem.size();
	allMem[mem->id] = mem;// put into mapping  	
	cout << "Pool Created with id\t" << mem->id << endl;	
	return VM_STATUS_SUCCESS;

	}
}

void idleFun(void*){ while(1){} }

//begin returns iterator
//front value of begin
//erase takes iterator
void schedule(){
	//cout << "scheduler" << endl;
	//cout << "current thread id: " << current << endl; 	
	TMachineSignalState oldstate;
	MachineSuspendSignals(&oldstate);
	if(!high.empty() && high.front()->id != current && (high.front())->state != VM_THREAD_STATE_WAITING && (high.front())->state != VM_THREAD_STATE_DEAD){
			//cout << "context switched to high!" << endl;
			(high.front())->state = VM_THREAD_STATE_RUNNING;
			TVMThreadID tmp =(high.front())->id;
			high.erase(high.begin());
			TVMThreadID prev = current;
			current = tmp;	
			MachineContextSwitch(&all[prev]->context, &all[current]->context);

	}
	

	else if(!normal.empty() && normal.front()->id != current && (normal.front())->state != VM_THREAD_STATE_WAITING && (normal.front())->state != VM_THREAD_STATE_DEAD){
			if((normal.front())->state == VM_THREAD_STATE_DEAD){ cout << "its dead " << (normal.front())->id << endl; }
			(normal.front())->state = VM_THREAD_STATE_RUNNING;
			//cout << current <<" context switched to normal! " << (normal.front())->id << endl;
			TVMThreadID tmp =(normal.front())->id;
			normal.erase(normal.begin());
			TVMThreadID prev = current;
			current = tmp;
			MachineContextSwitch(&all[prev]->context, &all[current]->context);
		
	}

	else if(!low.empty() && low.front()->id != current && (low.front())->state != VM_THREAD_STATE_WAITING && (low.front())->state != VM_THREAD_STATE_DEAD){
			//cout << "context switched to low!" << endl;
			(low.front())->state = VM_THREAD_STATE_RUNNING;

			TVMThreadID tmp =(low.front())->id;
			low.erase(low.begin());
			TVMThreadID prev = current;
			current = tmp; 
			MachineContextSwitch(&all[prev]->context, &all[current]->context);
	
	}
	else{

		///cout << "no threads found need to switch to idle!" << endl;
		TVMThreadID prev = current;
		current = idle; 
		MachineContextSwitch(&all[prev]->context, &all[current]->context);
	//	cout << "context switched" << endl;
	}

	MachineResumeSignals(&oldstate);
	return;

}

void Ready(TVMThreadID thread){
	//cout << "ready the thread: " << thread << " with thread priority " << all[thread]->priority << endl;
	all[thread]->state = VM_THREAD_STATE_READY;
	//cout << "this is normal size " << normal.size() << endl;	
	switch(all[thread]->priority){
		case VM_THREAD_PRIORITY_LOW: low.push_back(all[thread]); return;
		case VM_THREAD_PRIORITY_NORMAL: normal.push_back(all[thread]); return;
		case VM_THREAD_PRIORITY_HIGH: high.push_back(all[thread]); return;		
	} 
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
			//cout << "going to remove " << (*itr)->id << endl;
			(*itr)->state = VM_THREAD_STATE_DEAD;
			high.remove((*itr));
			//cout << " removed high " << endl;
			break;
	 		
			}
		}
	}
	if(!normal.empty()){
	for(itr = normal.begin(); itr != normal.end(); ++itr){
		if((*itr)->id == thread && (*itr)->state != VM_THREAD_STATE_DEAD){
			//cout << "going to remove " << (*itr)->id << endl;
			(*itr)->state = VM_THREAD_STATE_DEAD;
			normal.remove((*itr));
			//cout << " removed normal" << endl;
 			break;
			}
		}
	}

	//cout << "between normal and low " << endl;
	if(!low.empty()){
	for(itr = low.begin(); itr != low.end(); ++itr){
		if((*itr)->id == thread && (*itr)->state != VM_THREAD_STATE_DEAD){
			//cout << "going to remove " << (*itr)->id << endl;
			(*itr)->state = VM_THREAD_STATE_DEAD;
			low.remove((*itr));
			//cout << " removed low" << endl;
			break;
		}
	}}
	//cout << " terminated " << endl; 
	all[thread]->state = VM_THREAD_STATE_DEAD;
	schedule();
	VMThreadDelete(thread);
	//need one for low and normal and error checking according to specs
	 MachineResumeSignals(&oldstate);
	

	if(all[thread]->id == 0) {return VM_STATUS_ERROR_INVALID_ID;}
	else if(all[thread]->state != VM_THREAD_STATE_DEAD){return VM_STATUS_ERROR_INVALID_STATE;}
	else{return VM_STATUS_SUCCESS;}


}
TVMStatus VMThreadDelete(TVMThreadID thread){
	map<TVMThreadID, tcb*>::iterator itr;
	//map<TVMThreadID, tcb*>temp;

	if(all[thread]->id == 0) {return VM_STATUS_ERROR_INVALID_ID;}
	else if(all[thread]->state != VM_THREAD_STATE_DEAD){return VM_STATUS_ERROR_INVALID_STATE;}
	else{
		schedule();
		all.erase(current);		

		return VM_STATUS_SUCCESS;
	}


}
void Skeleton(void* params){
	cout << "entering skeleton" << endl;
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
		//cout << "ticks left " << (*itr)->ticks << endl; 
		(*itr)->ticks--;
		if((*itr)->ticks == 0) { 
	//		cout << "tock" << endl;
			(*itr)->state = VM_THREAD_STATE_READY;
			Ready((*itr)->id); //put into a ready queue
			sleeping.erase(itr);
			break;
		} else { itr++; } 
		
	}
	//cout << "trying to schedule now" << endl;
	schedule();
	//cout << "this coming here" << endl;
}

TVMStatus VMThreadSleep(TVMTick tick){
	//TMachineSignalState oldstate;
	//MachineSuspendSignals(&oldstate);
	all[current]->ticks = tick;//possibly have to multiply by 1000
	all[current]->state = VM_THREAD_STATE_WAITING;
	sleeping.push_back(all[current]); //a function that looks through threads and adds them to the sleep queue
	schedule();
	cout << "put thread  " << current << " to sleep with " << all[current]->ticks << " ticks"<< endl;
	//MachineResumeSignals(&oldstate);
	return(VM_STATUS_SUCCESS);
}

void MachineCallBack(void *calldata, int result);
//-1 means file wasn't written to and an error

TVMStatus VMFileWrite(int filedescriptor, void *data, int *length){

	int bytesToWrite = 0;
	TMachineSignalState oldstate;
	MachineSuspendSignals(&oldstate);
	int temp = *length;
	int written = 0;
	void *tempAddr;
	if (*length > 512){
			bytesToWrite = *length - temp;
			if(bytesToWrite > 512){bytesToWrite = 512;}
			VMMemoryPoolAllocate(1,512, &tempAddr);

		}
		else{
			VMMemoryPoolAllocate(1,(TVMMemorySize)(*length), &tempAddr);
			cout << "hell234o " << endl;
		//	cout << "this is length" << *length << endl;
		}

	while(temp > 0){
		memcpy (tempAddr, data, bytesToWrite);
		MachineFileWrite(filedescriptor, tempAddr, *length,MachineCallBack,(void*)current); //only thing the changes 

		all[current]->state = VM_THREAD_STATE_WAITING;
		schedule();
		//*length = all[current]->storeResult;
		temp = temp - 512;
		written += all[current]->storeResult;
		//if(temp <= 0){break;}
		data = (uint8_t *)data + bytesToWrite;
		cout << "hello " << endl;
		//cout << "this is temp " << temp << endl;
		//cout << "BytestoWrite " << bytesToWrite << endl;
		//cout << "this is data " << data << endl; 
	}
	
	//	VMMemoryPoolDeallocate(1,&tempAddr);
		cout << "why no here " << endl;
		MachineResumeSignals(&oldstate);
		return(VM_STATUS_SUCCESS); 
}

TVMStatus VMFileRead(int filedescriptor, void *data, int *length){
	TMachineSignalState oldstate;

	if((void*)data == NULL || *length == 0){
		return VM_STATUS_ERROR_INVALID_PARAMETER;	
	}	
	else{
		MachineSuspendSignals(&oldstate);
		MachineFileRead(filedescriptor, data, *length,MachineCallBack,(void*)current); //only thing the changes 
		all[current]->state = VM_THREAD_STATE_WAITING;
		schedule();
		*length = all[current]->storeResult;
		if(*length < 0){
			return VM_STATUS_FAILURE;		
		}
		else{
			return VM_STATUS_SUCCESS;	
		}
	}

}

TVMStatus VMFileClose(int filedescriptor){
	TMachineSignalState oldstate;

	MachineSuspendSignals(&oldstate);
	MachineFileClose(filedescriptor,MachineCallBack,(void*)current); //only thing the changes 
	all[current]->state = VM_THREAD_STATE_WAITING;
	schedule();
	//*length = all[current]->storeResult;
	if(all[current]->storeResult < 0){
		return VM_STATUS_FAILURE;		
	}
	else{
		return VM_STATUS_SUCCESS;	
	}
}


TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset){
	TMachineSignalState oldstate;

	MachineSuspendSignals(&oldstate);
	MachineFileSeek(filedescriptor, offset, whence,MachineCallBack,(void*)current); //only thing the changes 
	all[current]->state = VM_THREAD_STATE_WAITING;
	schedule();
	*newoffset = all[current]->storeResult;
	if(*newoffset < 0){
		return VM_STATUS_FAILURE;		
	}
	else{
		return VM_STATUS_SUCCESS;	
	}

}

TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor){
	TMachineSignalState oldstate;

	if(filename == NULL || filedescriptor == NULL){
		return VM_STATUS_ERROR_INVALID_PARAMETER;	
	}	
	else{

		MachineSuspendSignals(&oldstate);
		MachineFileOpen(filename, flags, mode,MachineCallBack,(void*)current); //only thing the changes 
		all[current]->state = VM_THREAD_STATE_WAITING;
		schedule();
		*filedescriptor  = all[current]->storeResult;
		//cout << "hello" << endl;
		if(*filedescriptor < 0){

			return VM_STATUS_FAILURE;		
		}
		else{
			return VM_STATUS_SUCCESS;	
		}
	}
}

//same one for all of them
void MachineCallBack(void *calldata, int result){ //current would be garage because the one we wanted gets moved 
	TMachineSignalState oldstate;
	MachineSuspendSignals(&oldstate); //suspend any interrupts
	//cout << "machine call back" << endl;
	Ready((TVMThreadID)calldata);
	//cout << "after" << endl;
	//cout << (TVMThreadID)calldata << "call data value" << endl;
	all[(TVMThreadID)calldata]->storeResult = result;
	MachineEnableSignals();
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
	cout << "Allocating Thread Stack" << endl;
	VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SYSTEM, thread->memsize, &thread->base);
	cout << "Thread Stack Allocated" <<endl;
	thread->priority = prio;
	thread->state = VM_THREAD_STATE_DEAD;
	//thread->base = new uint8_t[thread->memsize];
	all[thread->id] = thread;//added to map 	
	cout << "memsize from app " << memsize << endl;
	cout << "check map: " << " prio " << all[*tid]->priority << " memsize " << all[*tid]->memsize << endl;
	cout << "thread created "<< all[*tid]->id << " returning tid " << *tid  << " from  "  <<  thread->id  << " with thread state " << all[*tid]->state << endl;
	//need to create context
	
  	MachineResumeSignals(&oldstate);

	if((entry != NULL) & (tid != NULL)){return VM_STATUS_SUCCESS;}
	else{return VM_STATUS_ERROR_INVALID_PARAMETER;}
	
	
}

TVMStatus VMThreadActivate(TVMThreadID thread){
	//need to handle high priority 
	//need to check and see if its dead first
	cout << "Activate thread " << thread << endl;
	TMachineSignalState oldstate;
	MachineSuspendSignals(&oldstate);
	cout << "creating context" << endl; 
        MachineContextCreate(&(all[thread]->context), Skeleton, all[thread]->params, all[thread]->base, all[thread]->memsize); 		
	all[thread]->state = VM_THREAD_STATE_READY;
	cout << "activated thread: " << thread << " with a state of " << all[thread]->state << endl;   
	Ready(thread);//put into a ready queue 
	//schedule(); 	
	MachineResumeSignals(&oldstate);

	if(all[thread]->id == 0) {return VM_STATUS_ERROR_INVALID_ID;}
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

	if(all[thread]->id == 0){return VM_STATUS_ERROR_INVALID_ID;}
	else if(stateref == NULL){return VM_STATUS_ERROR_INVALID_PARAMETER;}
	else {return VM_STATUS_SUCCESS;}
}

