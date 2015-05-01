#include <iostream> 
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <string>
#include <vector>
using namespace std; 

void print_prompt();
vector<string> parse_input(string cmd);
vector<vector<string> >  create_process(vector<string> cmd);
void execute_cmds(vector<vector<string> > processes);

int main(void) 
{

	vector<string> cmds;
	vector<vector<string> > p;
	cout << "\tShell\t" << endl;
	
	while(1){
	print_prompt();
	string input;
	getline (cin, input);
	cmds = parse_input(input);
	p = create_process(cmds);
	execute_cmds(p);
	}
	
	

	return(0);
}
/*
*	Prints 16 character long prompt 
*/
void print_prompt()
{


	char buffer[128];
	getcwd(buffer,sizeof(buffer));
	string cwd(buffer);
	cout << cwd << "> ";
	
}
/*
*	All cmds are parsed with spaces removed
*/

vector<string> parse_input(string cmd){

	vector<string> process;
	string token; 
	string dem = " ";
	size_t pos;
	
	while((pos = cmd.find(dem)) != std::string::npos){
	   token = cmd.substr(0,pos);
	   cmd.erase(0, pos + 1);
	   process.push_back(token);
	  cout << "token " << token << endl;
	}
	 token = cmd.substr(0,pos);
	 cmd.erase(0, pos + 1);
	process.push_back(token);

	return(process);	
}
vector<vector<string> > create_process(vector<string> cmd){

	vector<vector<string> > processes;
	vector<string> tmp;
	cmd.push_back("|"); //for last element
	string dem = "|";
	vector<string>::iterator itr;

	for(itr=cmd.begin(); itr != cmd.end(); itr++){

	       if(*itr != "|"){
			tmp.push_back(*itr);
		//	cout << "pushed in temp vec " << *itr << endl;
		}
		else{
		  processes.push_back(tmp);
	       	  tmp.clear();
		}
	}//goes down the cmds

	return(processes);


}

void execute_cmds(vector<vector<string> >  processes){
	vector<vector<string> >::iterator itr;
	vector<string>::iterator i;
	for(itr=processes.begin(); itr != processes.end(); itr++)
	{	

		int pid = fork();
		int status;
		
		if(pid == 0){
			i = itr->begin();
			char* cmd = (char*)i->c_str();  //first word is a command
			vector<char*> arguments;
			for(; i != itr->end(); i++){
				arguments.push_back((char*)i->c_str());
			}
			cout << endl;
			arguments.push_back(NULL);
			char** argv = &arguments[0];
			int status = execvp(cmd, argv);
			if(status < 0)
				cerr << "if did not work!" << endl;
		}
		if(pid > 0)
		{

		 pid_t done = wait(&status);


		}

		
		
	}

}









