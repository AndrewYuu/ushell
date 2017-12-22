#include "helpers.h"
#include <stdlib.h>
#include <string.h>

char *replaceString(char *currentString, char *toReplace){
    char *pch;
    pch = strstr(currentString, toReplace);
    pch = pch + strlen(toReplace) - 1;
    strncpy(pch, "~", 1);
    return pch;
}

char *replaceHome(char *currentString, char *toReplace){
    char *pch;
    char *homeDir = getenv("HOME");
    pch = strstr(currentString, toReplace);
    pch = pch + strlen(toReplace) - 1;
    strncpy(pch, homeDir, 1);
    return pch;
}

char *getNetID(char *directory){
    return NULL;
}

char *getLastToken(char *ppromptTermBuf, char *delimiter){
    char *token = strtok(ppromptTermBuf, delimiter);
    char *prevtoken;
    while(token != NULL){
        prevtoken = token;
        token = strtok(NULL, delimiter);
    }
    char prompt[256] = "";
    strcpy(prompt, "~/");
    strcat(prompt, prevtoken);
    strcat(prompt, " :: anlyu >>");
    char *pprompt = prompt;
    return pprompt;
}
