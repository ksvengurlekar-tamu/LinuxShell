#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <fcntl.h> // required for open() + flags
#include <vector>
#include <string>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {
    // reset dup's before the shell executes
    dup(0);
    dup(1);

    // allocate space for the last known directory file path
    char prev[250];
    getcwd(prev, sizeof(prev));

    for (;;) {
        // need date/time, username, and absolute path to current dir
        
        // getenv("USER")
        string user = getenv("USER");

        // time()+ctime()
        time_t t = time(0);
        string time = ctime(&t);

        // getcwd() 
        char dir[250];
        getcwd(dir, sizeof(dir));

        // ubuntu shell format: yellow time, green user, blue file path
        cout << YELLOW << time << GREEN << user << WHITE << ":" << BLUE << dir << NC << "$ ";
        
        // get user inputted command
        string input;
        getline(cin, input);

        if (input == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError()) {  // continue to next prompt if input had an error
            continue;
        }


        // print out every command token-by-token on individual lines
        // prints to cerr to avoid influencing autograder
        for (auto cmd : tknr.commands) {
            for (auto str : cmd->args) {
                cerr << "|" << str << "| ";
            }
            if (cmd->hasInput()) {
                cerr << "in< " << cmd->in_file << " ";
            }
            if (cmd->hasOutput()) {
                cerr << "out> " << cmd->out_file << " ";
            }
            cerr << endl;
        }

        // handle cd command argument
        if (tknr.commands.at(0)->args[0] == "cd"){
            // cd - returns to the previous directory
            if (tknr.commands.at(0)->args[1] != "-"){
                getcwd(prev, sizeof(prev));
                
                int status = chdir(tknr.commands.at(0)->args[1].c_str());

                // error finding directory
                if(status != 0){
                    cout << "bash: cd: "<< tknr.commands.at(0)->args[1] <<": No such file or directory" << endl;
                }
            }
            else {
                chdir(prev);
            }

            continue;
        }

        for (auto command : tknr.commands) {
            int fd[2]; // multiple pipe implementation: new pipe for every command in loop
            
            if (pipe(fd) < 0) {
                perror("bad pipe creation");
                exit(2);
            }

            // fork to create child
            pid_t pid = fork();
            if (pid < 0) {  // error check
                perror("fork");
                exit(2);
            }

            if (pid == 0) {  // if child, exec to run command
                close(fd[0]);

                // need to handle < and > redirects
                // open will return a file descriptor; need to store and use in the pipe
                int read = 0;
                int write = 1;
                if (command->hasInput()) {
                    read = open(command->in_file.c_str(), O_RDONLY); // O_RDONLY = read only
                    dup2(read, 0);
                }

                if (command->hasOutput()) {
                    write = open(command->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU); // flags = write only, create on open, overwrite prev file on open, permissions flag
                    dup2(write, 1);
                }

                // handle background processes
                if (command != tknr.commands.back()){
                    dup2(fd[1], 1);
                }

                // separate args to be processed by exec
                char** args = new char* [command->args.size() + 1];
                for(size_t i = 0; i < command->args.size(); i++){
                    args[i] = (char*) command->args.at(i).c_str();
                }
                args[command->args.size()] = nullptr;

                if (execvp(args[0], args) < 0) {  // error check
                    perror("execvp");
                    exit(2);
                }
            }
            else {  // if parent, wait for child to finish
                dup2(fd[0], 0);
                close(fd[1]);

                int status = 0;
                if (!command->isBackground()) { // only need to wait for non-background processes
                    waitpid(pid, &status, 0);
                }
                else { // kill zombies
                    signal(SIGCHLD, SIG_IGN);
                }

                if (status > 1) {  // exit if child didn't exec properly
                    exit(status);
                }
            }
        }

        // sets the old fd standard in/out to the next
        dup2(3,0);
        dup2(4,1);
    }
}
