#include "VirtualMachine.h"
#include "Machine.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

//for testing 
#include <iostream> 
#include <cstdlib> 
using namespace std;
//VirtualMachineUtils.c functions made to work in C++
extern "C"{
	TVMMainEntry VMLoadModule(const char *module);
	TVMStatus VMFilePrint(int filedescriptor, const char *format, ...);
}

/*
*	Get VMStart to work, use MainEntry function pointer to load a module and then call it 
*/
TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[])
{
	TVMMainEntry vmmain; //creates a function pointer
	vmmain = VMLoadModule(argv[0]); //set function pointer to point to this function
	cout << "application: " << argv[0] << endl;
	if(vmmain == NULL)//check if vmmain is pointing to the address of VMLoadModule 
		cerr << "is not null!" << endl;
	else
		vmmain(argc, argv);//run the function it points to, in a way it derefences it

  	return(VM_STATUS_SUCCESS);

}



TVMStatus VMFileWrite(int filedescriptor, void *data, int *length){
	write(filedescriptor, data, *length);
	return(VM_STATUS_SUCCESS);
}

