// ECS 150
// Project 1
// 10/03/18

#include <fcntl.h> // dup2 system call
#include <unistd.h> // read and write system calls
#include <dirent.h> // opendir and readdir system calls
#include <sys/stat.h> // stat
#include <sys/types.h> // types
#include <sys/wait.h> // types
#include <termios.h> // noncanon
#include <stdlib.h> // for HOME environment variable
#include <ctype.h>
#include <cstring>
#include <vector>
#include <string>

#include "noncanmode.c" // set noncanon and reset canon functions

// this funtion prints out the dir in the front, such as  /.../ecs150%
void WriteOutDir() {
    char cwd[512];
    getcwd(cwd, sizeof(cwd));

    int len = (int)strlen(cwd);
    if (len > 16) {
        std::string last_dir = std::string(cwd);
        int place = 0;
        for (int i = len - 1; i > 0; i--) { // find place of last '/'
            if (cwd[i] == '/') {
                place = i;
                break;
            }
        }
        strcpy(cwd, "/...");
        std::string temp = last_dir.substr(place, len - place);
        strcat(cwd, temp.c_str());
    }
    strcat(cwd, "% ");
    write(STDOUT_FILENO, cwd, strlen(cwd));
}

// func: write to stdout the various permissions given a file stat struct
void PrintPermissions(struct stat fileStat) {

    char isDir[1] = {'d'};
    char rChar[1] = {'r'};
    char wChar[1] = {'w'};
    char xChar[1] = {'x'};
    char slash[1] = {'-'};
    char space[1] = {' '};

    //https://stackoverflow.com/questions/10323060/printing-file-permissions-like-ls-l-using-stat2-in-c

    if (S_ISDIR(fileStat.st_mode)) { // if file is a dir
        write(STDOUT_FILENO, isDir, 1);
    } else { // not a dir
        write(STDOUT_FILENO, slash, 1);
    }

    if (fileStat.st_mode & S_IRUSR) { // user can read
        write(STDOUT_FILENO, rChar, 1);
    } else {
        write(STDOUT_FILENO, slash, 1);
    }

    if (fileStat.st_mode & S_IWUSR) { // user can write
        write(STDOUT_FILENO, wChar, 1);
    } else {
        write(STDOUT_FILENO, slash, 1);
    }

    if (fileStat.st_mode & S_IXUSR) { // use can execute
        write(STDOUT_FILENO, xChar, 1);
    } else {
        write(STDOUT_FILENO, slash, 1);
    }

    if (fileStat.st_mode & S_IRGRP) { // group can read
        write(STDOUT_FILENO, rChar, 1);
    } else {
        write(STDOUT_FILENO, slash, 1);
    }

    if (fileStat.st_mode & S_IWGRP) { // group can write
        write(STDOUT_FILENO, wChar, 1);
    } else {
        write(STDOUT_FILENO, slash, 1);
    }

    if (fileStat.st_mode & S_IXGRP) { // group can execute
        write(STDOUT_FILENO, xChar, 1);
    } else {
        write(STDOUT_FILENO, slash, 1);
    }

    if (fileStat.st_mode & S_IROTH) { // others can read
        write(STDOUT_FILENO, rChar, 1);
    } else {
        write(STDOUT_FILENO, slash, 1);
    }

    if (fileStat.st_mode & S_IWOTH) { // others can write
        write(STDOUT_FILENO, wChar, 1);
    } else {
        write(STDOUT_FILENO, slash, 1);
    }

    if (fileStat.st_mode & S_IXOTH) { // others can execute
        write(STDOUT_FILENO, xChar, 1);
    } else {
        write(STDOUT_FILENO, slash, 1);
    }

    write(STDOUT_FILENO, space, 1); // write a space before writing the actual file name
}

void FindFile(const char *directory, std::string file, std::vector<std::string> &pathsToFile, bool &opened) {

    char newline[1] = {'\n'};
    std::string curDir(directory);
    std::string openError = "Failed to open directory \"" + std::string(directory) + "/\"";

    // http://pubs.opengroup.org/onlinepubs/009604599/functions/opendir.html
    // http://pubs.opengroup.org/onlinepubs/009695299/functions/readdir_r.html
    // http://pubs.opengroup.org/onlinepubs/009695299/functions/closedir.html
    DIR *dir;
    struct dirent *dp;

    if ((dir = opendir(directory)) == NULL) { // if failed to open
        write(STDOUT_FILENO, newline, 1);
        write(STDOUT_FILENO, openError.c_str(), openError.size());
        write(STDOUT_FILENO, newline, 1);
        opened = false; // to handle extra newline
        return;
    }

    while ((dp = readdir(dir)) != NULL) { // for each file in directory

        //https://stackoverflow.com/questions/10323060/printing-file-permissions-like-ls-l-using-stat2-in-c // how to even use stat
        struct stat fileStat;
        stat(dp->d_name, &fileStat);

        // https://stackoverflow.com/questions/16587183/list-files-and-their-info-using-stat // how to pass full path to stat()
        std::string fullPath = curDir + '/' + std::string(dp->d_name);

        if (stat(fullPath.c_str(), &fileStat) != -1) {
            if (S_ISDIR(fileStat.st_mode)) { // if file is a dir
                if (strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..")) { // don't recurse up and don't stay in the same dir
                    FindFile((curDir + "/" + std::string(dp->d_name)).c_str(), file, pathsToFile, opened);
                }
                // https://stackoverflow.com/questions/36419738/d-name-comparison-with-a-cstring-generating-core-dump
            } else if (!strcmp(dp->d_name, file.c_str())) { // if file is in current dir
                pathsToFile.push_back(curDir + "/" + std::string(file));

            } else { // not a dir and not file we're looking for
                continue;
            }
        }
    }

    closedir(dir);
}

std::string GetCommand(char * buffer, std::string command, std::vector<std::string>& history) {


    char backspace[4] = "\b \b";
    int location = (int) history.size(); // for history location
    bool last_his = false;

    while (1) { // keep reading a char

        //https://www.geeksforgeeks.org/input-output-system-calls-c-create-open-close-read-write/

        // clear up buffer and read in new
        strcpy(buffer, "   ");
        read(STDIN_FILENO, buffer, 3);

        //*** deal with return and tab ***
        if (buffer[0] == '\n') {
            return command;
        } else if (buffer[0] == '\t') {
            continue;
        }

        //*** deal with the arrows ***
        if (buffer[1] == '[') {
            if (buffer[2] == 'A' && location - 1 >= 0) {
                last_his = true;
                for (unsigned i = 0; i < command.size(); i++) { // delete any chars before up arrow
                    write(STDOUT_FILENO, backspace, strlen(backspace));
                }
                command.clear(); // clear deleted chars from command
                location--; // go to previous command position

                write(STDOUT_FILENO, history[location].c_str(), history[location].size()); // write previous command to stdout
                command = history[location]; // set command to prev command
            } else if (buffer[2] == 'B' && location + 1 <= (int) history.size() - 1) { // if down arrow and next command in history
                for (unsigned i = 0; i < command.size(); i++) { // delete any chars before down arrow
                    write(STDOUT_FILENO, backspace, strlen(backspace));
                }
                command.clear(); // clear deleted chars from command
                location++; // go to next command postion

                write(STDOUT_FILENO, history[location].c_str(), history[location].size()); // write next command to stdout
                command = history[location]; // set command to next command
            } else if (buffer[2] == 'B' && location + 1 == (int) history.size() && last_his) {
                for (unsigned i = 0; i < history[location].size(); i++) {
                    write(STDOUT_FILENO, backspace, strlen(backspace));
                }
                last_his = false;
                command.clear();
                location++;
            } else { // beyond end of list of commands
                char audio[2] = "\a";
                if (buffer[2] == 'A' || buffer[2] == 'B') { // only audio for up and down
                    write(STDOUT_FILENO, audio, strlen(audio));
                }
            }
            continue;
        }

        //*** deal with backspace ***
        if (buffer[0] == 0x7F) { // if char is a backspace

            if (command.size() > 0) { // make sure there is something in the command
                char backspace[4] = "\b \b";
                write(STDOUT_FILENO, backspace, strlen(backspace));
                command.pop_back();
            } else {
                char audio[2] = "\a";
                write(STDOUT_FILENO, audio, strlen(audio));
            }
            continue;
        }

        write(STDOUT_FILENO, buffer, 1);
        command.push_back(buffer[0]);

    } // while
} // GetCommand

void pwd() {
    char newline[1] = {'\n'};
    char cwd[512];
    // https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.bpxbd00/rtgtc.htm
    getcwd(cwd, sizeof(cwd)); // get current working directory from the getcwd call
    write(STDOUT_FILENO, cwd, strlen(cwd));
    write(STDOUT_FILENO, newline, 1); // write newline to stdout
}

void ls(std::string directory) {
    // http://pubs.opengroup.org/onlinepubs/009604599/functions/opendir.html
    // http://pubs.opengroup.org/onlinepubs/009695299/functions/readdir_r.html
    // http://pubs.opengroup.org/onlinepubs/009695299/functions/closedir.html

    char newline[1] = {'\n'};

    if (!directory.size()) {
        directory.push_back('.');
    }

    DIR *dir;
    struct dirent *dp;


    if ((dir = opendir(directory.c_str())) == NULL) {
        std::string openError = "Failed to open directory \"" + directory + "/\"";
        write(STDOUT_FILENO, openError.c_str(), openError.size());
        write(STDOUT_FILENO, newline, 1);

        return;
    }

    while ((dp = readdir(dir)) != NULL) { // for each fileName

        //https://stackoverflow.com/questions/10323060/printing-file-permissions-like-ls-l-using-stat2-in-c // how to even use stat
        struct stat fileStat;
        stat(dp->d_name, &fileStat);

        // https://stackoverflow.com/questions/16587183/list-files-and-their-info-using-stat // how to pass full path to stat()
        std::string fullPath = directory + '/' + std::string(dp->d_name);

        if (stat(fullPath.c_str(), &fileStat) != -1) {
            PrintPermissions(fileStat);
            write(STDOUT_FILENO, dp->d_name, strlen(dp->d_name)); // write filename to stdout
            write(STDOUT_FILENO, newline, 1);
        }
    }
    closedir(dir);
}

void cd(std::string directory) {
    int rc;
    char newline[1] = {'\n'};
    if (!directory.size()) {
        // http://pubs.opengroup.org/onlinepubs/009695399/functions/getenv.html
        const char *name = "HOME";
        char* homeDir;
        homeDir = getenv(name);
        // http://pubs.opengroup.org/onlinepubs/009695399/functions/chdir.html
        rc = chdir(homeDir);
    } else {
        rc = chdir(directory.c_str());
        if (rc < 0) {
            write(STDOUT_FILENO, newline, 1);
            char error_mes[26] = "Error changing directory.";
            write(STDOUT_FILENO, error_mes, 26);
        }
    }
    write(STDOUT_FILENO, newline, 1); // write newline to stdout
}

void ff(std::string file, std::string directory) {
    char newline[1] = {'\n'};

    if (!file.size()) {
        std::string fileError = "ff command requires a filename!";
        write(STDOUT_FILENO, fileError.c_str(), fileError.size());
        write(STDOUT_FILENO, newline, 1);
        return;
    }
    if (!directory.size()) {
        directory.push_back('.');
    }

    struct stat fileStat;
    stat(file.c_str(), &fileStat);
    if (S_ISDIR(fileStat.st_mode)) { // if file is a dir
        return;
    }
    // run the search
    std::vector<std::string> pathsToFile;
    bool openedAll = true; // to handle extra whitespace when didn't open a dir
    FindFile(directory.c_str(), file, pathsToFile, openedAll);
    if (unsigned size = (unsigned)pathsToFile.size()) { // if found paths
        if (openedAll) {
            //write(STDOUT_FILENO, newline, 1);
        }
        for (unsigned i = 0; i < size; i++) {
            write(STDOUT_FILENO, pathsToFile[i].c_str(), strlen(pathsToFile[i].c_str()));
            write(STDOUT_FILENO, newline, 1);
        }
    } else { // didn't find any such file
        if (openedAll) {
            write(STDOUT_FILENO, newline, 1);
        }
    }
}

void ExecCommand(std::string command) {

    char newline[1] = {'\n'};

    if (!command.size()) { // if command is empty
        write(STDOUT_FILENO, newline, 1); // write newline to stdout
        return;
    }

    std::string directory, file; // for parsing command
    unsigned startOfArg = 0;
    bool nextCharSpace = false;
    std::vector<std::string> tokens;

    // **************
    // parse the space-separated arguments into tokens
    // *************
    unsigned spaceCounter = 0;
    for (unsigned i = 0; i < command.size(); i++) {
        if (command.at(i) == ' ') {
            tokens.push_back(command.substr(startOfArg, i - startOfArg));
            startOfArg = i + 1;
            // ******
            // ****** Handle case when directory has a '\' at the end
            // ****** For commands like "ls ecs\ 150" or "cd ecs\ 150"
            // ******
        } else if (command.at(i) == '\\') { // if we see a '\'
            if (i + 1 < command.size()) {
                if (command.at(i + 1) == ' ') {
                    nextCharSpace = true;
                }
            }
            if (i > 0) { // if '\' not first element
                if (command.at(i - 1) != ' ') { // if char before was not a space
                    tokens.push_back(command.substr(startOfArg, i - startOfArg));
                    while (i + 1 < command.size()) { // while still more chars to append
                        if (command.at(i + 1) == ' ') { // if see a space
                            spaceCounter++; // add to counter
                            if (!nextCharSpace && spaceCounter) { // if next char after '\' wasn't a space stop at next space
                                startOfArg = i + 2; //*** set new arg position for rest of command
                                i += 1;
                                break;
                            }
                            if (spaceCounter == 2) { // if second space
                                startOfArg = i + 2; //*** set new arg position for rest of command
                                i += 1;
                                break; // stop appending to token
                            }
                        }
                        tokens[tokens.size() - 1] += command.at(i + 1);
                        i++;
                    }
                }
                continue;
            }
            if (i == 0 || command.at(i - 1) == ' ') { // if first element is '\' or prev char was a space
                std::string newString;
                while (i + 1 < command.size()) { // while still more chars
                    if (command.at(i + 1) == ' ') { // if see a space
                        spaceCounter++; // add to counter
                        if (!nextCharSpace && spaceCounter) { // if next char after '\' wasn't a space stop at next space
                            startOfArg = i + 2; //*** set new arg position for rest of command
                            i += 2;
                            break;
                        }
                        if (spaceCounter == 2) { // if second space
                            startOfArg = i + 2; //*** set new arg position for rest of command
                            i += 2;
                            break; // stop appending to token
                        }
                    }
                    newString += command.at(i + 1);
                    i++;
                }
                tokens.push_back(newString);
            }

        } else if (i == command.size() - 1) { // pick up the last arg
            tokens.push_back(command.substr(startOfArg, i - startOfArg + 1)); // + 1 to include lasts char
        }
    }

    // **************
    // parse each token further by any special characters ('|', '>', '<', '&')
    // *************
    // https://stackoverflow.com/questions/27375040/append-a-char-to-a-stdvectorstring-at-a-specific-location
    bool saw_special_char = false;
    for (unsigned i = 0; i < tokens.size(); i++) { // for each token
        for (unsigned j = 0; j < tokens.at(i).size(); j++) {  // for each char in token
            if ((tokens[i][j] == '|' || tokens[i][j] == '<' || tokens[i][j] == '>' || tokens[i][j] == '&')) {
                saw_special_char = true;
                std::string specialChar(1, tokens[i][j]);
                if (j < tokens[i].size() - 1) { // if the special char is not the last element of token
                    std::string temp = tokens[i].substr(j + 1, tokens[i].size() - j - 1); // save the rest of token
                    tokens.insert(tokens.begin() + (i + 1), temp); // insert it into next position of the tokens vector
                }
                tokens[i] = tokens[i].substr(0, j); // set the token to be the string before the special char
                tokens.insert(tokens.begin() + (i + 1), 1, specialChar); // insert the specialChar as a token on its own
                // https://www.hackerrank.com/challenges/vector-erase/problem
                if (j == 0) { // if the specialChar was the first element in the token
                    tokens.erase(tokens.begin() + i); // delete the inserted empty string
                }
            }
        }
    }

    //pipe
    if (saw_special_char) {

        const char *argv[tokens.size() + 1];// = {cmd.c_str(), NULL}; // for list of arguments // <-- change size to numArgs
        for (unsigned i = 0; i < tokens.size(); i++) {
            argv[i] = tokens.at(i).c_str();
        }
        argv[tokens.size()] = NULL;

        // start taking away < and >, check number of pipes need to do
        int num_pipes = 0;
        int fdin =dup(0), fdout = dup(1), infile, outfile = 0;
        for (unsigned i = 0; i < tokens.size(); i++) {
            if (tokens[i] == "<") {
                if (i + 1 < tokens.size()) {
                    infile = open(tokens[i + 1].c_str(), O_RDONLY);
                    if (infile == -1) {
                        //print out errror message
                        write(STDOUT_FILENO, newline, 1);
                        write(STDOUT_FILENO, "File \"", 6);
                        write(STDOUT_FILENO, tokens[i + 1].c_str(), tokens[i + 1].size());
                        write(STDOUT_FILENO, "\" does not exist!\n", 18);
                        return;
                    } else {
                        fdin = infile;
                        tokens.erase(tokens.cbegin() + i);
                        tokens.erase(tokens.cbegin() + i);
                        i--;
                    }
                } else{
                    tokens.erase(tokens.cbegin() + i);
                    i--;
                }
            } else if(tokens[i] == ">"){
                if (i + 1 < tokens.size()) {
                    outfile = open(tokens[i + 1].c_str(), O_WRONLY | O_CREAT );
                    fdout = outfile;
                    tokens.erase(tokens.cbegin() + i);
                    tokens.erase(tokens.cbegin() + i);
                    i--;
                } else{
                    tokens.erase(tokens.cbegin() + i);
                    i--;
                }
            } else if(tokens[i] == "|"){
                num_pipes++;
            }
        }

        int child_status;

        //https://stackoverflow.com/questions/8082932/connecting-n-commands-with-pipes-in-a-shell
        pid_t pid;
        int fdpipe[2];
        int location = 0;
        for (int times = 0; times < num_pipes; times++) {
            for (unsigned i = location ; i < tokens.size(); i ++) {
                if (tokens.at(i) == "|") {
                    location = i + 1;
                    for (unsigned m = location; m < i; m++) {
                        argv[m] = tokens.at(m).c_str();
                    }
                    argv[i] = NULL; //put needed args into arg
                    //done with argvs


                    pipe(fdpipe); // start the pipe,
                    fdout = fdpipe[1]; //need the write end of pipe
                    pid = fork(); // start new process
                    if (pid == 0) {

                        if (fdout != 1) { //output should be written into pipe
                            dup2(fdout, 1);
                            close(fdout);
                        }
                        if (fdin != 0) {
                            dup2(fdin, 0);
                            close(fdin);
                        }
                        if (std::string(argv[0]) == "pwd") {
                            pwd();
                            exit(0);
                        } else if(std::string(argv[0]) == "ls") {
                            if (argv[1] == NULL) {
                                argv[1] = ".";
                            }
                            ls(std::string(argv[1]));
                            exit(0);
                        } else if(std::string(argv[0]) == "ff"){
                            if (argv[2] == NULL) {
                                argv[2] = ".";
                            }
                            if (argv[1] == NULL) {
                              std::string fileError = "ff command requires a filename!";
                              write(STDOUT_FILENO, fileError.c_str(), fileError.size());
                              write(STDOUT_FILENO, newline, 1);
                              return;
                            }
                            ff(argv[1], argv[2]);
                            exit(0);
                        } else {
                            execvp(argv[0],(char*const*) argv); //start the command
                            std::string failMessage = "Failed to execute " + std::string(argv[0]);
                            write(STDOUT_FILENO, failMessage.c_str(), failMessage.size());
                            write(STDOUT_FILENO, newline, 1);
                            return;
                        }

                    } else {
                        //Wait for the child process to terminate before forking the next one.
                        waitpid(pid,NULL,0);

                    }
                    //for the next process
                    close(fdpipe[1]); // when last child process is done, don't need the write end anymore, close it
                    fdin = fdpipe[0]; // but the next child process needs to read from this pipe
                    break;
                }
            }
        }

        for (int i = 0; i < location; i ++) {
            tokens.erase(tokens.cbegin());
        }
        for (unsigned m = 0; m < tokens.size(); m++) {
            argv[m] = tokens.at(m).c_str();
        }
        argv[tokens.size()] = NULL; //put needed args into arg

        if (outfile != 0) {
            fdout = outfile;
        }
        pid = fork();
        if (pid == 0) { // child process
            write(STDOUT_FILENO, newline, 1);

            if (fdin != 0) {
                dup2(fdin, 0);
                close(fdin);
            }
            if (fdout != 1) { //output should be written into pipe
                dup2(fdout, 1);
                close(fdout);
            }

            if (std::string(argv[0]) == "pwd") {
                pwd();
                exit(0);
            } else if(std::string(argv[0]) == "ls") {
                if (argv[1] == NULL) {
                    argv[1] = ".";
                }
                ls(std::string(argv[1]));
                exit(0);
            } else if(std::string(argv[0]) == "ff"){
                if (argv[2] == NULL) {
                    argv[2] = ".";
                }
                if (argv[1] == NULL) {
                    std::string fileError = "ff command requires a filename!";
                    write(STDOUT_FILENO, fileError.c_str(), fileError.size());
                    write(STDOUT_FILENO, newline, 1);
                    return;
                }
                ff(argv[1], argv[2]);
                exit(0);
            } else {
                execvp(argv[0],(char*const*) argv); //start the command
                // If execvp returns, it must have failed.

                std::string failMessage = "Failed to execute " + std::string(argv[0]);
                write(STDOUT_FILENO, failMessage.c_str(), failMessage.size());
                write(STDOUT_FILENO, newline, 1);
                return;
            }

        } else { // parent process
            // This is run by the parent. Wait for the child process
            //to terminate

            pid_t tpid;
            do { //http://www.cs.ecu.edu/karl/4630/sum01/example1.html
                tpid = wait(&child_status);
            } while(tpid != pid);
        }
        return;
    }

    // *******************
    // ******************* deal with all the internal commands
    // *******************
    if (tokens[0] == "pwd") {
        write(STDOUT_FILENO, newline, 1);
        pwd();
    }
    else if (tokens[0] == "ls") {

        if (tokens.size() > 1) {
            directory = tokens[1];
        }
        write(STDOUT_FILENO, newline, 1);
        ls(directory);
    }
    else if (tokens[0] == "cd") {
        if (tokens.size() > 1) {
            directory = tokens.at(1);
        }
        cd(directory);
    }
    else if (tokens[0] == "ff") {

        if (tokens.size() > 1) { // file specified
            file = tokens[1];
        }

        if (tokens.size() > 2) { // directory specified
            directory = tokens[2];
        }
        write(STDOUT_FILENO, newline, 1);
        ff(file, directory);
    }

    else { // Not an internal command

        const char *argv[tokens.size() + 1];
        for (unsigned i = 0; i < tokens.size(); i++) {
            argv[i] = tokens.at(i).c_str();
        }
        argv[tokens.size()] = NULL;

        // http://www.cs.ecu.edu/karl/4630/sum01/example1.html
        int child_status;
        pid_t pid = fork();
        if (pid == 0) { // child process
            write(STDOUT_FILENO, newline, 1);
            execvp(argv[0], (char*const*) argv);

            // If execvp returns, it must have failed.
            std::string failMessage = "Failed to execute " + tokens[0];
            write(STDOUT_FILENO, failMessage.c_str(), failMessage.size());
            write(STDOUT_FILENO, newline, 1);
            exit(1);

        } else { // parent process
            // This is run by the parent. Wait for the child process
            //to terminate
            pid_t tpid;
            do { //http://www.cs.ecu.edu/karl/4630/sum01/example1.html
                tpid = wait(&child_status);
            } while(tpid != pid);
        }
    }

} // function ExecCommand end

void ParseCommand(std::string& command) {
    std::string new_one;
    bool sp = false;
    for (unsigned i = 0; i < command.size(); i++) {
        if (isspace(command.at(i))) {
            if (sp) {
                new_one.push_back(' ');
                sp = false;
            }
        } else {
            new_one.push_back(command[i]);
            sp = true;
        }
    }

    if (new_one[new_one.size() - 1] == ' ') {
        new_one.pop_back();
    }
    command = new_one;
}


int main(int argc, char *argv[]) {

    char buffer[3];
    struct termios SavedTermAttributes;

    std::string unparsedCommand, command;
    std::vector<std::string> history;

    SetNonCanonicalMode(STDIN_FILENO, &SavedTermAttributes);

    WriteOutDir();

    while(1) { // keep getting commands

        // get command from command line
        command = unparsedCommand = GetCommand(buffer, command, history);
        ParseCommand(command);

        if (command == "exit") { // stop getting commands
            char newline[1] = {'\n'};
            write (STDOUT_FILENO, newline, 1);
            break;
        }
        ExecCommand(command); // run command

        //store the history command in the string vector history
        if (history.size() >= 10) {
            history.erase(history.cbegin());
        }
        if (unparsedCommand.size()) { // place the entered command into history
            history.push_back(unparsedCommand);
        }
        command.clear(); // clear command
        WriteOutDir();
    }

    ResetCanonicalMode(STDIN_FILENO, &SavedTermAttributes);
    return 0;
}



//https://www.cs.purdue.edu/homes/grr/SystemsProgrammingBook/Book/Chapter5-WritingYourOwnShell.pdf
