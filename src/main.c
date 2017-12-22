#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <readline/readline.h>
#include <signal.h>

#include "sfish.h"
#include "debug.h"
#include "helpers.h"



int main(int argc, char *argv[], char* envp[]) {
    char* input;
    char inputBuff[1024] = "";
    bool exited = false;

    char absPath[256] = "";
    char *pabsPath = absPath;

    char promptTermBuf[256] = "";
    char delim[5] = {' ', '/'};
    char *promptTerm;

    char *homeDirectory = getenv("HOME");
    char previousPathToCd[256] = "";


    if(!isatty(STDIN_FILENO)) {
        // If your shell is reading from a piped file
        // Don't have readline write anything to that file.
        // Such as the prompt or "user input"
        if((rl_outstream = fopen("/dev/null", "w")) == NULL){
            perror("Failed trying to open DEVNULL");
            exit(EXIT_FAILURE);
        }
    }

    do {
        if(strcmp(getcwd(pabsPath, 256), homeDirectory) >= 0){
            promptTerm = replaceString(getcwd(pabsPath, 256), homeDirectory);
        }
        else{
            promptTerm = getcwd(pabsPath, 256);
        }
        strcpy(promptTermBuf, promptTerm);
        strcat(promptTermBuf, " :: anlyu >> ");
        getcwd(pabsPath, 256);

        // getcwd(promptTermBuf, 256);
        // const char *currDirectoryToken = getLastToken(promptTermBuf, delim);

        promptTerm = promptTermBuf;
        input = readline(promptTerm);
        strcpy(inputBuff, input);

        // If EOF is read (aka ^D) readline returns NULL
        if(input == NULL || strcmp(input, "") == 0) {
            continue;
        }
        if(input != NULL){

            //THIS WILL GET THE FIRST COMMAND: pwd, cd, help, or exit FOR BUILTINS
            char *parseInput = strtok(input, delim);
            if(parseInput == NULL){
                continue;
            }

            // if(strcmp(parseInput, "pwd") == 0){
            //     printf("%s\n", getcwd(pabsPath, 256));
            // }

            if(strcmp(parseInput, "cd") == 0){
                //READ THE NEXT ARGUMENT IN THE INPUT. THIS IS THE PATH INDICATED BY THE USER
                parseInput = strtok(NULL, " ");

                if(parseInput == NULL){
                    //NAVIGATE TO THE HOME DIRECTORY
                    strcpy(previousPathToCd, pabsPath);
                    chdir(homeDirectory);
                }
                else{
                    char firstCharacter = *parseInput;
                    char nextCharacter = *(parseInput + 1);
                    //IF FOLLOWING THE cd COMMAND IS NOTHING (JUST cd)

                    if(strcmp(parseInput, "-") == 0){
                        chdir(previousPathToCd);
                        strcpy(previousPathToCd, pabsPath);
                    }
                    else if(strcmp(parseInput, ".") == 0){ //IF THE ENTIRE DIRECTORY IS JUST '.'
                        chdir(".");
                        //MAINTAIN THE PREVIOUS VALID DIRECTORY THAT WAS cd'ed TO. COPY THE STRING INTO NEW BUFFER. IF POINTER, THEN THE PREVIOUS STRING WILL BE OVERWRITTEN BECAUSE IT IS STORED IN A BUFFER.
                        strcpy(previousPathToCd, pabsPath);
                    }
                    else if(strcmp(parseInput, "..") == 0){ //IF THE ENTIRE DIRECTORY IS JUST '.'
                        chdir("..");
                        //MAINTAIN THE PREVIOUS VALID DIRECTORY THAT WAS cd'ed TO. COPY THE STRING INTO NEW BUFFER. IF POINTER, THEN THE PREVIOUS STRING WILL BE OVERWRITTEN BECAUSE IT IS STORED IN A BUFFER.
                        strcpy(previousPathToCd, pabsPath);
                    }
                    else if(firstCharacter == '.' && nextCharacter == '.'){
                        int dir = chdir(parseInput);
                        if(dir == 0){
                            //MAINTAIN THE PREVIOUS VALID DIRECTORY THAT WAS cd'ed TO. COPY THE STRING INTO NEW BUFFER. IF POINTER, THEN THE PREVIOUS STRING WILL BE OVERWRITTEN BECAUSE IT IS STORED IN A BUFFER.
                            strcpy(previousPathToCd, pabsPath);
                        }
                        if(dir != 0){
                            printf(BUILTIN_ERROR, "Invalid Directory");
                        }
                    }
                    else if(firstCharacter == '.'){
                        int dir = chdir(parseInput);
                        if(dir == 0){
                            strcpy(previousPathToCd, pabsPath);
                        }
                        if(dir != 0){
                            printf(BUILTIN_ERROR, "Invalid Directory");
                        }
                    }
                    else{
                        firstCharacter = *parseInput;
                        int dir;
                        if(firstCharacter != '/'){
                            //IF THERE IS NO LEADING '/', ADD IT TO THE FRONT OF THE USER TYPED DIRECTORY STRING
                            char *temp = strdup(parseInput);
                            strcpy(parseInput, "./");
                            strcat(parseInput, temp);
                            dir = chdir(parseInput);
                        }
                        else{
                            // //FIND THE homeDirectory SUBSTRING FROM THE parseInput STRING AND POINT TO IT.
                            // //IF THE homeDirectory SUBSTRING IS NOT PART OF THE PATH, IT IS AN INVALID DIRECTORY.
                            // char *pathToCd = strstr(parseInput, homeDirectory);
                            // //CHANGE THE DIRECTORY TO THE NEW USER SELECTED DIRECTORY.
                            dir = chdir(parseInput /*pathToCd*/);
                        }
                        if(dir == 0){
                            strcpy(previousPathToCd, pabsPath);
                        }
                        if(dir != 0){
                            printf(BUILTIN_ERROR, "Invalid Directory");
                        }
                    }
                }
            }

            // else if(strcmp(parseInput, "help") == 0){
            //     printf(USAGE);
            // }
            else if(strcmp(parseInput, "jobs") == 0){
                int statValue = 0;
                statValue = executeProgram(inputBuff);
                if(statValue < 0){
                    printf(EXEC_NOT_FOUND, inputBuff);
                }
            }

            else if(strcmp(parseInput, "exit") == 0){
                exit(EXIT_SUCCESS);
            }

            else{
                //EXECUTE PROGRAM
                int statValue = 0;
                statValue = executeProgram(inputBuff);

                if(statValue < 0){
                    printf(EXEC_NOT_FOUND, inputBuff);
                }
            }

            // Readline mallocs the space for input. You must free it.
            rl_free(input);
        }

    } while(!exited);

    debug("%s", "user entered 'exit'");

    return EXIT_SUCCESS;
}
