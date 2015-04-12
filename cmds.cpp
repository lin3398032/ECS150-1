#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <sys/types.h>

#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <ctype.h>
#include <cstring>
#include <string>
#include <list>
#include <vector>
#include <iterator>

#include <iostream> //for testing
using namespace std;


int main(void){

    struct dirent *dir;
    DIR *p;

    p = opendir (".");
    if (p == NULL) {
     printf ("Cannot open directory");
     return 1;
    }

    while ((dir = readdir(p)) != NULL) {
   	 write(STDIN_FILENO, dir->d_name,  (unsigned)strlen(dir->d_name));
   	 write(STDIN_FILENO, "\n", sizeof("\n"));

        
    }
    closedir (p);

}