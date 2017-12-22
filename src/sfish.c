#include "sfish.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>


volatile pid_t pid;
int child_counter = 0;
char *delim = " ";
char ioInDelim[1] = {'<'};
char ioOutDelim[1] = {'>'};
char ioBothDelim[2] = {'<', '>'};
char *pipeDelim = "|";
char ioBothAndPipeDelim[3] = {'<','|','>'};
char jidDelim[1] = {'%'};
volatile int pipeCounter;
int countNumberOfPipes;
int invalidPipes;
Process* head;
Process* current;
int jobCounter = 0;

//SIGNAL HANDLER. HOW SIGCHLD IS HANDLED. IN THE SIG HANDLER, WAIT FOR THE CHILDREN TO DIE IN A WHILE LOOP.
//THEN THE CHILDREN ARE REAPED ONE BY ONE. pid IS 0 WHEN THERE ARE NO CHILDREN TO BE REAPED. SHELL IS IN SUSPEND MODE.
//pid IS NON 0 AFTER wait REAPS A CHILD PROCESS.
void sigchild_handler(int sig){

    //if(child_counter > 0){
        while((pid = waitpid(-1, NULL, WNOHANG)) > 0){
            //CHILD IS REAPED
            child_counter--;
            debug("child counter in sigchild handler: %d", child_counter);
            //IF THE HEAD IS PAUSED, YOU DONT WANT TO MOVE THE HEAD, WHICH WOULD REMOVE THE PAUSED PROCESS.
            if(head != NULL){
                if(head->isPaused == 0 && head->nextProcess != NULL){
                    head = head->nextProcess;
                }
                else if(head->isPaused == 0 && head->nextProcess == NULL){
                    head = NULL;
                }
            }
            debug("the pid of the process to remove is: %d", pid);

            debug("after\n");
        }
        debug("exited\n");
    //}
}

void sigint_handler(int sig){
}

void sigtstp_handler(int sig){
    //LOOP THROUGH LINKED LIST OF PROCESSES AND CHECK WHICH ONES ARE RUNNING IN isRunning
    //PAUSE THE PROCESSES THAT ARE RUNNING WITH ITS RESPECTIVE PID IN THE STRUCT.
    Process* current_process = current;
    debug("sigtstp handler");
    while(current_process != NULL){
        if(current_process->isRunning){
            //GET THE CURRENT PROCESS PID AND PAUSE IT.
            debug("current process pid: %d", current_process->pid);
            kill(current_process->pid,SIGTSTP);
            current_process->isRunning = 0;
            current_process->isPaused = 1;
            if(current_process->nextProcess != NULL){
                current = current_process->nextProcess;
            }
        }
        current_process = current_process->nextProcess;
        child_counter--;
        debug("child counter in sigtstp handler after pause: %d", child_counter);
    }
}

int executeProgram(char *input){
    child_counter = 0;
    struct stat file;
    int statVal = -1;
    int i = 0;
    int isRedirect = 0;
    int isPipe = 0;
    int beingRepeated_1 = 0;
    int beingRepeated_2 = 0;
    int noArguments = 0;
    char **stringBuff = calloc(1024, 1024);
    char *tempCopy[1024];
    char absPath[256] = "";
    char *path = absPath;
    int fd[2];
    pipeCounter = 0;
    countNumberOfPipes = 0;
    invalidPipes = 0;
    sigset_t mask, prevMask;

    //SIG_HANDLERS INSTALL
    signal(SIGCHLD, sigchild_handler);
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
    //ZERO OUT MASK BIT VECTOR.
    sigemptyset(&mask);
    //RAISE SIGCHLD FLAG, SIGINT FLAG, AND SIGTSTP FLAG.
    //THUS, WHEN PASSED INTO SIGSUSPEND, THE SUSPEND WILL RESPOND AND RETURN TO THESE SIGNALS. IGNORES ALL OTHER SIGNALS.
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTSTP);

    //IGNORE SIGINT FOR THE SHELL
    signal(SIGINT, SIG_IGN);

    //CHECK FOR REPEATS OF '<' OR '>'
    beingRepeated_1 = isRepeated(input, '<');
    beingRepeated_2 = isRepeated(input, '>');
    noArguments = noArg(input);
    //COUNT THE TOTAL NUMBER OF PIPES
    countNumberOfPipes = countPipes(input);
    if(invalidPipes){
        printf(SYNTAX_ERROR, "Redirection failed.");
        return 0;
    }

    if(beingRepeated_1 || beingRepeated_2){
        printf(SYNTAX_ERROR, "Redirection failed.");
        return 0;
    }

    if(noArguments){
        printf(SYNTAX_ERROR, "Redirection failed.");
        return 0;
    }

    //CREATE THE LINKED LIST OF PROCESSES
    if(strstr(input, "|") != NULL){
            isPipe = 1;
    }

    if(strcmp(input, "jobs") == 0){
        Process *firstJob = head;
        debug("head argument: %s, pid: %d", firstJob->args, firstJob->pid);
        while(firstJob != NULL){
            if(firstJob->jid >= 0 && firstJob->isPaused == 1){
                printf(JOBS_LIST_ITEM, firstJob->jid, firstJob->args);
            }
            firstJob = firstJob->nextProcess;


            if(firstJob != NULL){
                debug("next argument: %s, pid: %d", firstJob->args, firstJob->pid);
            }


        }
        return 0;
    }


    if(strstr(input, "fg") != NULL){
        toForeground(input);
        return 0;
    }

    if(strstr(input, "kill") != NULL){
        toKill(input);
        return 0;
    }

    //PERSERVE THE INPUT
    char *temp;

    //PIPES HAVE THE HIGHEST PRECEDENCE, SO TOKENIZE THE PIPES FIRST.
    temp = strtok(input, pipeDelim);
    addJobToList(temp);

    Process *first_process = current; //PROCESS TO RUN
    Process *this_process = current; //PROCESS TO SET THE PIPES.

    //SET PIPES
    if(isPipe){
        //WHILE THE pipeCounter IS LESS THAN THE TOTAL NUMBER OF PIPES,
        //SET THE CURRENT PROCESS' OUT DESCRIPTOR AND THE NEXT PROCESS' IN DESCRIPTOR
        while(pipeCounter < countNumberOfPipes){
            //pipe => CURRENT PROGRAMS OUT TO NEXT PROGRAMS IN.
            pipe(fd);
            this_process->out_descriptor = fd[1];
            if(this_process->nextProcess != NULL){
                this_process->nextProcess->in_descriptor = fd[0];
            }
            pipeCounter++;
            this_process = this_process->nextProcess;
        }
    }

    pipeCounter = 0;
    //WHILE THE ARRAY OF PROCESSES IS NOT NULL
    while(first_process != NULL){
        debug("current_process ARGUMENTS: %s", first_process->args);
        //COMMANDS AND ARGS WITH ABSOLUTE FILE PATH
        if(*(first_process->args) == '/'){

            if(strstr(first_process->args, ">") != NULL || strstr(first_process->args, "<") != NULL){
                isRedirect = 1;
            }

            char *temp;
            //PERSERVE input AFTER IT IS MODIFIED BY STRTOK
            *tempCopy = strdup(first_process->args);

            temp = strtok(first_process->args, delim);
            *(stringBuff + i) = strdup(temp);
            i++;
            statVal = stat(*stringBuff, &file);
            //IF THE FILE EXISTS
            if(statVal == 0){
                //FORK AND EXECUTE
                sigprocmask(SIG_BLOCK, &mask, &prevMask); //BLOCK SIGCHLD. THIS IS TO PREVENT INFINITELY WAITING IN THE PARENT PROCESS.
                pid = fork();
                child_counter++;
                if(pid == 0){
                    sigprocmask(SIG_SETMASK, &prevMask, NULL);
                    //UNIGNORE SIGINT FOR THE CHILD
                    signal(SIGINT, SIG_DFL);
                    //DONE BY CHILD (FORK RETURNS 0 FOR pid)
                    if(isRedirect){
                        //CLEAR stringBuff
                        memset(stringBuff, 0, sizeof(*stringBuff));
                        redirect(*tempCopy, stringBuff);
                    }
                    if(isPipe){
                        pipeLine(*tempCopy, stringBuff, first_process);
                    }
                    if(!isRedirect && !isPipe){
                        while((temp = strtok(NULL, delim))!= NULL){
                            *(stringBuff + i) = strdup(temp);
                            i++;
                        }
                    }
                    execvp(*(stringBuff), stringBuff);
                    //IF execvp RETURNS, FAILED TO RUN OR FIND DIRECTORY.
                    debug("child process");
                    printf(EXEC_NOT_FOUND, first_process->args);
                    exit(0);
                }
                else{
                    //DONE BY PARENT (FOR RETURNS CHILD PID FOR pid). WAITS UNTIL CHILD PROCESS IS FINISHED.
                    debug("child counter: %d", child_counter);
                    first_process->pid = pid; //GET THE pid OF THE CURRENT FOREGROUND CHILD PROCESS THAT IS RUNNING.
                    int pidTemp = pid;
                    first_process->isRunning = 1; //THE PROCESS IS RUNNING
                    while(child_counter > 0){ //WHEN THE NUMBER OF CHILDREN REACHES 0 (ALL CHILDREN REAPED, DONE IN THE CHILD HANDLER), THE PROGRAM (SHELL) NO LONGER NEEDS TO BE SUSPENDED.
                        debug("suspending prog");
                        sigsuspend(&prevMask); //SUSPENDS, PAUSES THE PROGRAM UNTIL SIGCHILD IS RECEIVED.
                    }
                    // first_process->isRunning = 0; //WHEN SHELL STOPS SUSPENDING, THAT MEANS THE PROCESS FINISHED RUNNING.
                    sigprocmask(SIG_SETMASK, &prevMask, NULL);
                    //CLOSE PIPE DESCRIPTORS SO THAT THE NEXT PROCESS IN THE PIPE CAN RUN AND SUCCESSFULLY EXIT (NOT STALL)
                    if(child_counter == 0 && first_process->isPaused == 0){
                        if(first_process->in_descriptor > 0){
                            close(first_process->in_descriptor);
                        }
                        if(first_process->out_descriptor > 0){
                            close(first_process->out_descriptor);
                        }
                    }
                    first_process = first_process->nextProcess;
                    if(current->isPaused == 0){
                        removeJobFromList(pidTemp);
                        current = NULL;
                    }
                    if(first_process != NULL){
                        current = first_process;
                    }
                }
            }
        }


        //OTHER COMMANDS AND ARGS INCLUDING help AND pwd
        else{
            debug("current_process ARGUMENTS: %s", first_process->args);
            statVal = 0;
            if(strstr(first_process->args, ">") != NULL || strstr(first_process->args, "<") != NULL){
                isRedirect = 1;
            }
            //SEARCH FOR THE FILE THEN FORK AND EXEC.
            sigprocmask(SIG_BLOCK, &mask, &prevMask); //BLOCK SIGCHLD


            pid = fork();
            child_counter++;
            if(pid == 0){
                sigprocmask(SIG_SETMASK, &prevMask, NULL);
                //UNIGNORE SIGINT FOR THE CHILD
                signal(SIGINT, SIG_DFL);
                //DONE BY CHILD (FORK RETURNS 0 FOR pid)
                if(isRedirect){
                    redirect(first_process->args, stringBuff);
                }
                if(isPipe){
                    debug("args: %s", first_process->args);
                    debug("in DESCRIPTOR: %d", first_process->in_descriptor);
                    debug("out DESCRIPTOR: %d", first_process->out_descriptor);
                    pipeLine(first_process->args, stringBuff, first_process);
                }

                char *argCheckBuiltIn = strdup(first_process->args);
                if(strcmp((temp = strtok(argCheckBuiltIn, delim)), "pwd") == 0){
                    debug("child counter in child: %d", child_counter);
                    //EXEC PWD
                    printf("%s\n", getcwd(path, 256));
                }
                char *argCheckBuiltIn_2 = strdup(first_process->args);
                if(strcmp((temp = strtok(argCheckBuiltIn_2, delim)), "help") == 0){
                    //EXEC HELP
                    printf(USAGE);
                }

                if(!isRedirect && !isPipe){
                    char *temp;
                    temp = strtok(first_process->args, delim);
                    *(stringBuff + i) = strdup(temp);
                    i++;
                    while((temp = strtok(NULL, delim))!= NULL){
                        *(stringBuff + i) = strdup(temp);
                        i++;
                    }
                }
                if(strcmp(*(stringBuff), "pwd") != 0 && strcmp(*(stringBuff), "help") != 0){
                    execvp(*(stringBuff), stringBuff);
                    //IF execvp RETURNS, FAILED TO RUN OR FIND DIRECTORY.
                    debug("child process");
                    printf(EXEC_NOT_FOUND, first_process->args);
                }
                exit(0);
            }
            else{
                debug("child counter in parent: %d", child_counter);
                first_process->pid = pid; //GET THE pid OF THE CURRENT FOREGROUND CHILD PROCESS THAT IS RUNNING.
                int pidTemp = pid;
                first_process->isRunning = 1; //THE PROCESS IS RUNNING
                //DONE BY PARENT (FORK RETURNS CHILD PID FOR pid). WAITS UNTIL CHILD PROCESS IS FINISHED.
                while(child_counter > 0){ //WHEN THE NUMBER OF CHILDREN REACHES 0 (ALL CHILDREN REAPED, DONE IN THE CHILD HANDLER), THE PROGRAM (SHELL) NO LONGER NEEDS TO BE SUSPENDED.
                    debug("child counter before sigsuspend: %d", child_counter);
                    sigsuspend(&prevMask); //SUSPENDS, PAUSES THE PROGRAM UNTIL SIGCHILD IS RECEIVED.
                }
                debug("return sigsuspend");
                // first_process->isRunning = 0; //WHEN SHELL STOPS SUSPENDING, THAT MEANS THE PROCESS FINISHED RUNNING.
                sigprocmask(SIG_SETMASK, &prevMask, NULL);

                //CLOSE PIPE DESCRIPTORS SO THAT THE NEXT PROCESS IN THE PIPE CAN RUN AND SUCCESSFULLY EXIT (NOT STALL)
                if(child_counter == 0 && first_process->isPaused == 0){
                    if(first_process->in_descriptor > 0){
                        close(first_process->in_descriptor);
                    }
                    if(first_process->out_descriptor > 0){
                        close(first_process->out_descriptor);
                    }
                }
                first_process = first_process->nextProcess;
                if(current->isPaused == 0){
                    removeJobFromList(pidTemp);
                    current = NULL;
                }
                if(first_process != NULL){
                    current = first_process;
                }
            }

        }
        pipeCounter++;
    }

    free(stringBuff);
    return statVal;
}

void redirect(char *input, char **stringBuff){
    char *temp;
    char *inputText;
    char *outputText;
    char *trimmedInput;
    int fileDescriptor;
    int i = 0;
    // char path[256];

    //IO IN
    if(strstr(input, "<") != NULL && strstr(input, ">") == NULL){
        //TOKENIZE FOR REDIRECTION
        temp = strtok(input, ioInDelim);
        trimmedInput = strdup(temp);
        i++;
        while((temp = strtok(NULL, ioInDelim))!= NULL){
            //TOKENIZE TO REMOVE THE SPACES
            temp = strtok(temp, delim);
            //SET INPUT TEXT
            inputText = strdup(temp);
            i++;
        }
        //TOKENIZE FOR EXECUTION AND ARGUMENTS
        i = 0;
        temp = strtok(trimmedInput, delim);
        *(stringBuff + i) = strdup(temp);
        i++;
        while((temp = strtok(NULL, delim)) != NULL){
            *(stringBuff + i) = strdup(temp);
            i++;
        }
        fileDescriptor = open(inputText, O_RDONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        if(fileDescriptor == -1){
            printf(SYNTAX_ERROR, "Invalid File");
            exit(0);
        }
        dup2(fileDescriptor, 0);
    }

    //IO OUT
    if(strstr(input, ">") != NULL && strstr(input, "<") == NULL){

        //TOKENIZE FOR REDIRECTION
        temp = strtok(input, ioOutDelim);
        trimmedInput = strdup(temp);
        i++;
        while((temp = strtok(NULL, ioOutDelim))!= NULL){
            //TOKENIZE TO REMOVE THE SPACES
            temp = strtok(temp, delim);
            //SET OUTPUT TEXT
            outputText = strdup(temp);
            i++;
        }
        //TOKENIZE FOR EXECUTION AND ARGUMENTS
        i = 0;
        temp = strtok(trimmedInput, delim);
        *(stringBuff + i) = strdup(temp);
        i++;
        while((temp = strtok(NULL, delim)) != NULL){
            *(stringBuff + i) = strdup(temp);
            i++;
        }
        //SET IO REDIRECTION FILES
        fileDescriptor = open(outputText, O_TRUNC|O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        dup2(fileDescriptor, 1);
    }

    //BOTH
    if(strstr(input, ">") != NULL && strstr(input, "<") != NULL){
        char *tracker = input;
        char *firstIO = 0;
        char *secondIO = 0;
        int isFirst = 0;
        int fileDescriptor_in;
        int fileDescriptor_out;

        //CHECK WHICH SYMBOL COMES FIRST
        while(*tracker != 0){
            if((*tracker == '>') && isFirst != 0){
                secondIO = ">";
            }
            if((*tracker == '<') && isFirst != 0){
                secondIO = "<";
            }
            if((*tracker == '>') && isFirst == 0){
                firstIO = ">";
                isFirst = 1;
            }
            if((*tracker == '<') && isFirst == 0){
                firstIO = "<";
                isFirst = 1;
            }
            if(firstIO != 0 && secondIO != 0){
                break;
            }
            tracker = tracker + 1;
        }

        //IF INPUT THEN OUTPUT (IF FIRST SYMBOL IS "<", AND NEXT SYMBOL IS ">")
        //FIRST READ/GET INPUT FROM FILE, EXECUTE PROGRAM WITH THAT INPUT, AND THEN OUTPUT IT TO SPECIFIED FILE
        if(strcmp(firstIO, "<") == 0 && strcmp(secondIO, ">") == 0){
            temp = strtok(input, ioBothDelim);
            trimmedInput = strdup(temp);
            i++;
            while((temp = strtok(NULL, ioBothDelim))!= NULL){
                //TOKENIZE AND SET INPUT AND OUTPUT
                inputText = strdup(temp);
                temp = strtok(NULL, ioBothDelim);
                outputText = strdup(temp);

                //TOKENIZE TO REMOVE THE SPACES
                inputText = strtok(inputText, delim);
                inputText = strdup(inputText);
                outputText = strtok(outputText, delim);
                outputText = strdup(outputText);
                i++;
            }

            //TOKENIZE FOR EXECUTION AND ARGUMENTS
            i = 0;
            temp = strtok(trimmedInput, delim);
            *(stringBuff + i) = strdup(temp);
            i++;
            while((temp = strtok(NULL, delim)) != NULL){
                *(stringBuff + i) = strdup(temp);
                i++;
            }

            //SET IO REDIRECTION FILES
            fileDescriptor_in = open(inputText, O_RDONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
            dup2(fileDescriptor_in, 0);
            if(fileDescriptor_in == -1){
                printf(SYNTAX_ERROR, "Invalid File");
                exit(0);
            }
            fileDescriptor_out = open(outputText, O_TRUNC|O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
            dup2(fileDescriptor_out, 1);
        }

        //IF OUTPUT THEN INPUT (IF FIRST SYMBOL IS "<", AND NEXT SYMBOL IS ">")
        //(SAME CASE AS ABOVE, JUST REORDERED)STDOUT IS STILL SET TO OUTPUT FILE AND STDIN IS STILL SET TO INPUT FILE
        if(strcmp(firstIO, ">") == 0 && strcmp(secondIO, "<") == 0){
        temp = strtok(input, ioBothDelim);
            trimmedInput = strdup(temp);
            i++;
            while((temp = strtok(NULL, ioBothDelim))!= NULL){
                //TOKENIZE AND SET INPUT AND OUTPUT
                outputText = strdup(temp);
                temp = strtok(NULL, ioBothDelim);
                inputText = strdup(temp);

                //TOKENIZE TO REMOVE THE SPACES
                inputText = strtok(inputText, delim);
                inputText = strdup(inputText);
                outputText = strtok(outputText, delim);
                outputText = strdup(outputText);
                i++;
            }

            //TOKENIZE FOR EXECUTION AND ARGUMENTS
            i = 0;
            temp = strtok(trimmedInput, delim);
            *(stringBuff + i) = strdup(temp);
            i++;
            while((temp = strtok(NULL, delim)) != NULL){
                *(stringBuff + i) = strdup(temp);
                i++;
            }

            //SET IO REDIRECTION FILES
            fileDescriptor_in = open(inputText, O_RDONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
            if(fileDescriptor_in == -1){
                printf(SYNTAX_ERROR, "Invalid File");
                exit(0);
            }
            dup2(fileDescriptor_in, 0);
            fileDescriptor_out = open(outputText, O_TRUNC|O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
            dup2(fileDescriptor_out, 1);
        }
    }
    return;
}

void addJobToList(char *temp){
    int jobsAddedToList = 0;
    do{
        Process *new_process;
        // Process *current_process;
        new_process = (Process *)malloc(sizeof(Process));
        new_process->args = strdup(temp);
        new_process->nextProcess = NULL;
        new_process->prevProcess = NULL;
        new_process->pid = 0;
        new_process->isRunning = 0;
        new_process->isPaused = 0;
        new_process->jid = jobCounter;
        //IF THIS IS THE FIRST PROCESS IN THE LL
        if(head == NULL){
            head = new_process;
            current = new_process;
        }
        else if(head != NULL && current == NULL){
            Process *tempLastProcessInLL = head;
            while(tempLastProcessInLL->nextProcess != NULL){
                tempLastProcessInLL = tempLastProcessInLL->nextProcess;
            }
            tempLastProcessInLL->nextProcess = new_process;
            new_process->prevProcess = tempLastProcessInLL;
            current = tempLastProcessInLL->nextProcess;
        }
        else{
            current->nextProcess = new_process;
            new_process->prevProcess = current;
            current = new_process;
        }
        jobCounter++;
        jobsAddedToList++;
    }while((temp = strtok(NULL, pipeDelim)) != NULL);

    //MOVE THE CURRENT PROCESS POINTER TO THE CORRECT CURRENT PROCESS AFTER ADDING THE NEW PROCESS(ES).
    while(jobsAddedToList > 1){
        current = current->prevProcess;
        jobsAddedToList--;
    }
}

void removeJobFromList(pid_t pid){
    Process *current_process;
    // if(current != NULL){
    //     current_process = current;
    // }
    // else{
        current_process = head;
    // }
    while(current_process != NULL){
        if(current_process->pid == pid){
            current_process->isRunning = 0;
            //REMOVE THE JOB FROM THE LIST
            if(current_process->prevProcess != NULL && current_process->nextProcess != NULL){
                //THIS CAN HAPPEN IF A PREVIOUS PROCESS IS PAUSED AND CURRENT PROCESS IS REAPED
                current_process->nextProcess->prevProcess = current_process->prevProcess;
                current_process->prevProcess->nextProcess = current_process->nextProcess;
            }
            else if(current_process->nextProcess != NULL){
                current_process->nextProcess->prevProcess = NULL;
            }
            else if(current_process->prevProcess != NULL){
                current_process->prevProcess->nextProcess = NULL;
            }
            if(current_process == head){
                head = current_process->nextProcess;
            }
            if(current_process == current){
                current = NULL;
            }
            current_process->nextProcess = NULL;
            current_process->prevProcess = NULL;
            free(current_process);
            return;
        }
        current_process = current_process->nextProcess;
    }
    debug("exited removeJobFromList\n");
}

Process *goToFirstJob(){
    Process *firstProcess = head;
    while(firstProcess->prevProcess != NULL){
        firstProcess = firstProcess->prevProcess;
    }
    return firstProcess;
}

void toForeground(char *input){
    char *fg = strtok(input, delim);
    char *jidVal = strtok(NULL, delim);

    if(strcmp(fg, "fg") != 0){
        printf(BUILTIN_ERROR, "To foreground error.");
        return;
    }

    char *jid_char = strtok(jidVal, jidDelim);
    int jid = atoi(jid_char);
    Process *current_process = head;
    while(current_process != NULL){
        if(current_process->jid == jid){
            //RESUME JID
            child_counter++;
            //ZERO OUT MASK BIT VECTOR.
            sigset_t mask, prevMask;
            sigemptyset(&mask);
            //RAISE SIGCHLD FLAG, SIGINT FLAG, AND SIGTSTP FLAG.
            //THUS, WHEN PASSED INTO SIGSUSPEND, THE SUSPEND WILL RESPOND AND RETURN TO THESE SIGNALS. IGNORES ALL OTHER SIGNALS.
            sigaddset(&mask, SIGCHLD);
            sigaddset(&mask, SIGINT);
            sigaddset(&mask, SIGTSTP);

            kill(current_process->pid, SIGCONT);
            // sigprocmask(SIG_BLOCK, &mask, &prevMask);
            sigsuspend(&prevMask);
            sigprocmask(SIG_SETMASK, &prevMask, NULL);

            current = current_process;
            // waitpid(current_process->pid, NULL, WNOHANG);
            current_process->isPaused = 0;
            current_process->isRunning = 1;


            return;
        }
        current_process = current_process->nextProcess;
    }
    printf(BUILTIN_ERROR, "Invalid JID/PID to Kill.");
    return;
}

void toKill(char *input){
        char *killCommand = strtok(input, delim);
        char *idVal = strtok(NULL, delim);

        if(strcmp(killCommand, "kill") != 0){
            printf(BUILTIN_ERROR, "Kill Error");
            return;
        }

        //ITS A JID
        if(*idVal == '%'){
            char *jid_char = strtok(idVal, jidDelim);
            int jid = atoi(jid_char);
            Process *current_process = head;
            while(current_process != NULL){
                if(current_process->jid == jid){
                    //KILL JID
                    kill(current_process->pid, SIGKILL);
                    // waitpid(current_process->pid, NULL, WNOHANG);
                    removeJobFromList(current_process->pid);
                    return;
                }
                current_process = current_process->nextProcess;
            }
        }
        //ITS A PID
        else{
            //KILL PID
            Process *current_process = head;
            int process_pid = atoi(idVal);
            while(current_process != NULL){
                if(current_process->pid == process_pid){
                    //KILL PID
                    kill(current_process->pid, SIGKILL);
                    // waitpid(current_process->pid, NULL, WNOHANG);
                    removeJobFromList(current_process->pid);
                    return;
                }
                current_process = current_process->nextProcess;
            }
        }
        printf(BUILTIN_ERROR, "Invalid JID/PID to Kill.");
        return;
}

int isRepeated(char *input, char checkThisChar){
    char *checkPointer = input;
    int totalRepeat = 0;
    while(*checkPointer != '\0'){
        if(*checkPointer == checkThisChar){
            totalRepeat++;
        }
        if(totalRepeat > 1){
            return 1;
        }
        checkPointer++;
    }
    return 0;
}

int noArg(char *input){
    char *copy = strdup(input);
    char *args = strtok(copy, ioBothAndPipeDelim);
    if(args == NULL){
        return 1;
    }
    return 0;
}

int countPipes(char *input){
    char *checkPointer = input;
    int totalPipes = 0;
    while(*checkPointer != '\0'){
        if(*checkPointer == '|'){
            if(*(checkPointer + 1) == '|'){
                //THERE IS A REPEATED PIPE --> "||"
                invalidPipes = 1;
                return 0;
            }
            totalPipes++;
        }
        checkPointer++;
    }
    return totalPipes;
}

void pipeLine(char *input, char **stringBuff, Process *current_process){
    //SET DESCRIPTORS. 1ST COMMAND = STDOUT MODIFY (EVERY ODD COMMAND)
    //2ND COMMAND = STDIN MODIFY (EVERY EVEN COMMAND)
    int i = 0;
    char *temp;
    temp = strtok(input, delim);
    *(stringBuff + i) = strdup(temp);
    i++;
    while((temp = strtok(NULL, delim)) != NULL){
        *(stringBuff + i) = strdup(temp);
        i++;
    }
    //SET IO REDIRECTION FILES
    //IF FIRST PROGRAM, ONLY SET OUTPUT DESCRIPTOR
    if(pipeCounter == 0 /*pipeCounter % 2 == 0*/){
        dup2(current_process->out_descriptor, 1);
    }
    //IF LAST PROGRAM, ONLY SET INPUT DESCRIPTOR
    if(pipeCounter == countNumberOfPipes)/*else*/{
        dup2(current_process->in_descriptor, 0);
    }
    //IF MIDDLE PROGRAMS, SET BOTH DESCRIPTORS.
    else{
        dup2(current_process->out_descriptor, 1);
        dup2(current_process->in_descriptor, 0);
    }
    return;
}