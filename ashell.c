#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <sys/stat.h>

#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <sys/types.h>

#include <fcntl.h>
#include <ctype.h>
#include <cstring>
#include <string>
#include <list>
#include <vector>
#include <iterator>

#include <iostream> //for testing
using namespace std;

void ls_cmd(){
    struct dirent *dir;
    DIR *p;
    struct stat statbuf;
    p = opendir (".");
    if (p == NULL) {
     printf ("Cannot open directory");
     return 1;
    }

    while ((dir = readdir(p)) != NULL) {

	if (stat(dir->d_name, &statbuf) == -1)
      		  continue;
	cout << statbuf.st_mode << endl;
   	 write(STDIN_FILENO, p->d_name,  p->d_name.strlen());
        
    }
    closedir (p);


}
void history(list<string> cmds)
{
    list<string>::iterator itr; 
    int i = 0;
    for(itr=cmds.begin(); itr != cmds.end(); itr++)
    {   
        write(STDIN_FILENO, &i, sizeof(i));
        write(STDIN_FILENO, itr->c_str(), itr->length());
        i++;
    }

}

vector<string> parse_cmds(string line)
{
    stringstream ss(line);
    string s;
    vector<string> cmds;
   // vector<string>::iterator citr;

    while (std::getline(ss, s, ' ')) {
//	cout << "\ncmd: " <<  s << endl;
	cmds.push_back(s);

    }
    return cmds;
}

void execute_cmds(string line,list<string> oldCmds)
{

    vector<string> lookUp;
    lookUp.push_back("exit");
    lookUp.push_back("history");
    vector<string> cmds;
    vector<string>::iterator citr;
    vector<string>::iterator itr = lookUp.begin();
    cmds = parse_cmds(line);

	//parse through the commands that were entered 
	//as of right now we don't need a look up table!
	for(citr = cmds.begin(); citr != cmds.end(); citr++){ //it goes through everything that's entered
		for(itr = lookUp.begin(); itr != lookUp.end(); itr++){ //goes through the lookup table
			if(strcmp((*citr).c_str(), "exit") == 0 ){// C++ string vs C string
				exit(0); 
			}

			else if(strcmp((*citr).c_str(), "history") == 0){
				//history(oldCmds);	
                break;			
			}
			
			else if(strcmp((*citr).c_str(), "cd") == 0){
				cout << "CD" << endl;
			}

			else if(strcmp((*citr).c_str(), "pwd") == 0){
				cout << "pwd" << endl;
			}
			
			else if(strcmp((*citr).c_str(), "ls") == 0) {
				cout << "ls" << endl;
			}
			
			else{
				cout << "command invalid" << endl;
			}
		}	
	}
}

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
void down_callback(list<string> &vec,  list<string>::iterator &itr, string &line){
    if(vec.empty()){
        write(STDIN_FILENO, "\a", sizeof("\a") );
        //move back to prompt? Should already be at prompt
    }
    else if(itr == vec.begin())
    {
        write(STDIN_FILENO, "\a", sizeof("\a"));
    } else {
	cout << line.length();
        for(unsigned int i = 0; i < line.length(); i++)
        { //erase whatever is on the screen
            write(STDIN_FILENO, "\b \b", sizeof("\b \b"));
            
        }
        itr--;
        line.erase(line.begin(),line.end());
        line =  *itr;
        write(STDIN_FILENO, itr->c_str(), itr->length());
        
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



int main(void){
    struct termios SavedTermAttributes;
    char RXChar;
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    string line = "";


    int charOnLine = 0;
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
            execute_cmds(line, history);
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
		charOnLine++;
            }
            //if not printable then it is a backspace
            else if(0x08 == RXChar || 0x7F == RXChar){
		if(charOnLine){
                	write(STDIN_FILENO, "\b \b", sizeof("\b \b"));
			// dumps when there is no line need an if statement 
			// if line is empty() 
               		line.erase(line.size() - 1);
			charOnLine--;
		}
		else
  		      write(STDIN_FILENO, "\a", sizeof("\a"));
            }
           /* else{
                write(STDIN_FILENO, &RXChar, 1);
            }*/
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
