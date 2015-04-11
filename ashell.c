#include <stdio.h>
#include <stdlib.h>
#include <sstream>

#include <unistd.h>
#include <termios.h> 
#include <dirent.h>


#include <ctype.h>
#include <cstring>
#include <string> 
#include <list>
#include <iterator>

#include <iostream> //for testing 
using namespace std;
/*
string* parse_cmds(string line, string* lptable)
{
	stringstream ss(line);
	string s;
	string *cmds;
	int counter = 0;
	while (getline(ss, s, ' ')) {
 	 	cmds[counter] = s;
		counter++;
	}
	return cmds;
}

void execute_cmds(string line, string *lptable)
{
	string cmds = parse_cmds(line, lptable);
	 

}
*/
//void history_util(){ //loop through data structure and print out the list}
void up_callback(list<string> &vec, list<string>::iterator &itr, string &line)
{
	
//increment only when up is pressed and it is not empty or will seg fault
	if(vec.empty()){
		write(STDIN_FILENO, "\a", sizeof("\a") );
	//move back to prompt? Should already be at prompt
	}
	else if(itr == vec.end())
	{
		write(STDIN_FILENO, "\a", sizeof("\a"));
	} else {
		for(unsigned int i = 0; i < line.length(); i++)
		{ //erase whatever is on the screen
			write(STDIN_FILENO, "\b \b", sizeof("\b \b"));

		}
		line.erase(line.begin(),line.end());
		line = *itr;
		write(STDIN_FILENO, itr->c_str(), itr->length());
		itr++;
	}

	
}
void down_callback(list<string> vec,  list<string>::iterator itr, string &line){
	if(vec.empty()){
		write(STDIN_FILENO, "\a", sizeof("\a") );
	//move back to prompt? Should already be at prompt
	}
	else if(itr == vec.end())
	{
		write(STDIN_FILENO, "\a", sizeof("\a"));
	} else {
		for(unsigned int i = 0; i < line.length(); i++)
		{ //erase whatever is on the screen
			write(STDIN_FILENO, "\b \b", sizeof("\b \b"));

		}
		line.erase(line.begin(),line.end());
		line = *itr;
		write(STDIN_FILENO, itr->c_str(), itr->length());
		itr--;
	}

	
	
}
string build_path(){
	string path = ""; 
	path += '\n'; 
	path += getcwd(NULL, 0);
	path += '>';
	path += ' ';
	path += '\t';
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
	string line = "";
	//string lookUp [2] = {"exit", "ls"};

	string path = build_path();
	
	//build_path() makes it to class specs 

	
	//history
	list<string> history;
	list<string>::iterator itr = history.begin();


	SetNonCanonicalMode(STDIN_FILENO, &SavedTermAttributes);



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
		//add command to history list
		history.insert(itr++, line);
		//run commands
		//execute_cmds(line, lookUp); 
		//clean up
		line = "";
		itr = history.begin();
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
				up_callback(history, itr, line );
			}
			//down
			else if(0x42 == RXChar){
				down_callback(history, itr, line);
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


//http://www.cs.cornell.edu/Courses/cs414/2004su/homework/shell/shell.html
//http://www.fantabooks.net/lib/Unix%20Systems%20Programming%20Communication,%20Concurrency,%20and%20Threads%202003/0130424110/ch11lev1sec1.php

    	//read char out and also write to file? If enter is pressed look if it actually records something?
		//http://www.csee.umbc.edu/portal/help/theory/ascii.txt
		//http://www.lafn.org/~dave/linux/terminalIO.html
		//http://uw714doc.sco.com/en/SDK_sysprog/TDC_Non-CanonicalModeInProc.html
		//http://stephen-brennan.com/2015/01/16/write-a-shell-in-c/
