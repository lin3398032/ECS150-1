#include <stdio.h>
#include <stdlib.h>

# include <fcntl.h>
#include <unistd.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/stat.h> 
#include <sys/wait.h>
# include <sys/mman.h>
#include <sys/file.h> 

#include <cstring> 

#include <string> 
#include <vector>

#include <iostream> //for testing 
using namespace std;

int main(int argc, char *argv[]){
	
	const size_t region_size = sysconf(_SC_PAGE_SIZE); //set to system's page size 
	string str = "Kyle";
	const char *name = str.c_str();	
	
	
	int i = shm_open ( name, O_CREAT | O_EXCL | O_RDWR , S_IRUSR | S_IWUSR );
	if(i < 0){
		
		cerr << "shared memory failed\n" << endl;
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