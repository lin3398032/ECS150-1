#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <termios.h> 
#include <dirent.h>


#include <ctype.h>
#include <cstring>
#include <string> 
#include <list>


#include <iostream> //for testing 
using namespace std;

void up_callback)(list<string> list){
	cout << "\tUP\t"; cout.flush();
	
	
}
void down_callback(list<string> list){
		cout << "\down\t"; cout.flush();

	
	
}
string build_path(){
	string path = getcwd(NULL, 0);
	path += '>';
	path += '\r';
	return path;
	//add 16 character limiter
}
void ResetCanonicalMode(int fd, struct termios *savedattributes){
    tcsetattr(fd, TCSANOW, savedattributes);
}

void SetNonCanonicalMode(int fd, struct termios *savedattributes){
    struct termios TermAttributes;
    char *name;
    
    // Make sure stdin is a terminal. 
    if(!isatty(fd)){
        fprintf (stderr, "Not a terminal.\n");
        exit(0);
    }
    
    // Save the terminal attributes so we can restore them later. 
    tcgetattr(fd, savedattributes);
    
    // Set the funny terminal modes. 
    tcgetattr (fd, &TermAttributes);
    TermAttributes.c_lflag &= ~(ICANON | ECHO); // Clear ICANON and ECHO. 
    TermAttributes.c_cc[VMIN] = 1;
    TermAttributes.c_cc[VTIME] = 0;
    tcsetattr(fd, TCSAFLUSH, &TermAttributes);
}


int main(int argc, char *argv[]){
	struct termios SavedTermAttributes;
	char RXChar;
	DIR *dir = NULL;
	struct dirent *entry = NULL;
	string path = build_path();
	

	//build_path() makes it to class specs 

	
	//history
	list<string> history;
	

	SetNonCanonicalMode(STDIN_FILENO, &SavedTermAttributes);


	string line = "";
	write(STDIN_FILENO, path.c_str(), path.capacity());
    while(1){
	
        read(STDIN_FILENO, &RXChar, 1);
        if(0x04 == RXChar){ // Ctrl - D 
	    line += '\0';
	    write(STDIN_FILENO, line.c_str(), line.length());
	   // cout << line; cout.flush();
            break;
		}
		if(0x0A == RXChar){
			write(STDIN_FILENO, path.c_str(), path.length());
		
			
		}
		//Check for up and down
		//space
		if(0x1B == RXChar){
			//left bracket
			read(STDIN_FILENO, &RXChar, 1);
			if(0x5B == RXChar){
				read(STDIN_FILENO, &RXChar, 1);
				//up
				if(0x41 == RXChar){
					up_callback(history);
				}
				//down
				else if(0x42 == RXChar){
					down_callback(history);
				}
			}
		}

		//not any of this is then a character 
        else{
            if(isprint(RXChar)){
		//check for backspace
				write(STDIN_FILENO, &RXChar, sizeof(RXChar));
				line += RXChar;
            }
			else if(0x08 == RXChar || 0x7F == RXChar){
				write(STDIN_FILENO, "\b \b", sizeof("\b \b"));

				
			}
            else{
		write(STDIN_FILENO, &RXChar, 1);
            }
        }
 	}
    
    ResetCanonicalMode(STDIN_FILENO, &SavedTermAttributes);
	cout << "\n" << endl;
	
    return 0;
}





    	//read char out and also write to file? If enter is pressed look if it actually records something?
		//http://www.csee.umbc.edu/portal/help/theory/ascii.txt
		//http://www.lafn.org/~dave/linux/terminalIO.html
		//http://uw714doc.sco.com/en/SDK_sysprog/TDC_Non-CanonicalModeInProc.html
		//http://stephen-brennan.com/2015/01/16/write-a-shell-in-c/
