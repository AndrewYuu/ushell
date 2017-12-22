#ifndef SFISH_H
#define SFISH_H

#include <unistd.h>

/* Format Strings */
#define EXEC_NOT_FOUND "sfish: %s: command not found\n"
#define JOBS_LIST_ITEM "[%d] %s\n"
#define STRFTIME_RPRMT "%a %b %e, %I:%M%p"
#define USAGE " exit [n]\n help [-dms] [pattern ...]\n cd [-L|[-P [-e]] [-@]] [dir]\n pwd [-LP]\n"
#define BUILTIN_ERROR  "sfish builtin error: %s\n"
#define SYNTAX_ERROR   "sfish syntax error: %s\n"
#define EXEC_ERROR     "sfish exec error: %s\n"

int executeProgram(char *input);
int sigchld_handler(int sig);
void sigint_handler(int sig);
void sigtstp_handler(int sig);
void redirect(char *input, char **stringBuff);

int isRepeated(char *input, char checkThisChar);
int countPipes(char *input);
void addJobToList(char* temp);
void removeJobFromList(pid_t pid);
void toKill(char *input);
void toForeground(char *input);
int noArg(char *input);

struct process{
    char *args;
    int in_descriptor;
    int out_descriptor;
    int pid;
    int jid;
    int isRunning;
    int isPaused;
    struct process *nextProcess;
    struct process *prevProcess;
};
typedef struct process Process;

Process *goToFirstJob();

void pipeLine(char *input, char **stringBuff, Process *current_process);

#endif

