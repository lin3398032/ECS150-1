#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>

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
    struct stat fileStat;
    //need to make this a parameterized thing
    //might have to convert dir->name to string it to string

    p = opendir ("./");
    if (p == NULL) {
     printf ("Cannot open directory");
     return 1;
    }

    while ((dir = readdir(p)) != NULL) {
   	 write(STDIN_FILENO, dir->d_name,  (unsigned)strlen(dir->d_name));
   	 write(STDIN_FILENO, "\n", sizeof("\n"));
   	 stat(dir->d_name, &fileStat); //negative value on failure, need to add directory
        
    }
    closedir (p);

}

/*


The ls command you will be doing internally needs to do the following:
1. open the directory specified (if none specified open up ./) use the opendir from dirent.h 
2. for each entry in the directory:
  2.1 read in the entry using readdir (actually readdir will return NULL if at the end so might be while loop)
  2.2 get the permissions of the entry using the ->d_name and a call to stat (see the stat system call also don't forget to concatenate the directory name before calling stat)
  2.3 output the permissions rwx, etc then output the entry name ->d_name
3. close the directory using closedir
 
The history command you will just need to do the following:
1. for each previously executed command (up to 10):
  1.1 output the item number 0 - 9 (you can do '0' + index to get the integer, no need to use some conversion of integer to string)
  1.2 output the command as a string
 
Does this make sense? Have you tested the example shell?

http://pubs.opengroup.org/onlinepubs/7908799/xsh/stat.html
http://pubs.opengroup.org/onlinepubs/7908799/xsh/sysstat.h.html
http://codewiki.wikidot.com/c:system-calls:stat
http://linux.die.net/man/2/stat


*/