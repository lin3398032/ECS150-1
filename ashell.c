#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <termios.h> 
#include <dirent.h>


#include <ctype.h>
#include <cstring>
#include <string> 
#include <vector>


#include <iostream> //for testing 
using namespace std;
void execute_cmd(){


}
//void history_util(){ //loop through data structure and print out the vector}
void up_callback(vector<string> vec, vector<string>::iterator itr){
//increment only when up is pressed and it is not empty or will seg fault
	if(vec.empty()){
		write(STDIN_FILENO, "\a", sizeof("\a") );
	//move back to prompt? Should already be at prompt
	}
	else if(++itr == vec.end())
	{
		cout << "\a";cout.flush();
	} else {
	
		itr++;
		write(STDIN_FILENO, itr->c_str(), itr->length());

	}
	
	
}
void down_callback(vector<string> vec,  vector<string>::iterator itr){
		cout << "\tdown\t"; cout.flush();

	
	
}
string build_path(){
	string path = ""; 
	path += '\n'; 
	path += getcwd(NULL, 0);
	path += '>';

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
	vector<string> history;
	vector<string>::iterator itr;


	SetNonCanonicalMode(STDIN_FILENO, &SavedTermAttributes);


	string line = "";
	write(STDIN_FILENO, path.c_str(), path.length());
    while(1){

	//reads character
        read(STDIN_FILENO, &RXChar, 1);
        if(0x04 == RXChar){ // Ctrl - D 
	 line += '\0';
	    //write(STDIN_FILENO, line.c_str(), line.length());
            break;
	}
	else if(0x0A == RXChar){
		//enter

		line += '\0';
		history.push_back(line);
		itr = history.begin();
		//add command to history vector
		//run commands 
	

		write(STDIN_FILENO, path.c_str(), path.length());
	}
	//Check for up and down
	//space
	else if(0x1B == RXChar){
		//left bracket
		read(STDIN_FILENO, &RXChar, 1);
		if(0x5B == RXChar){
			read(STDIN_FILENO, &RXChar, 1);
			//up
			if(0x41 == RXChar){
				up_callback(history, itr);
			}
			//down
			else if(0x42 == RXChar){
				down_callback(history, itr);
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
			//if not printable then it is a backspace
			else if(0x08 == RXChar || 0x7F == RXChar){
				write(STDIN_FILENO, "\b \b", sizeof("\b \b"));
			}
            else{
				write(STDIN_FILENO, &RXChar, 1);
            }
        }
 	}
    
    ResetCanonicalMode(STDIN_FILENO, &SavedTermAttributes);
	//cout << "\n" << endl;
	
    return 0;
}



//http://www.fantabooks.net/lib/Unix%20Systems%20Programming%20Communication,%20Concurrency,%20and%20Threads%202003/0130424110/ch11lev1sec1.php

    	//read char out and also write to file? If enter is pressed look if it actually records something?
		//http://www.csee.umbc.edu/portal/help/theory/ascii.txt
		//http://www.lafn.org/~dave/linux/terminalIO.html
		//http://uw714doc.sco.com/en/SDK_sysprog/TDC_Non-CanonicalModeInProc.html
		//http://stephen-brennan.com/2015/01/16/write-a-shell-in-c/
