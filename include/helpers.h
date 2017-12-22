#ifndef HELPERS_H
#define HELPERS_H

char *replaceString(char *currentString, char *toReplace);

char *replaceHome(char *currentString, char *toReplace);

char *getNetID(char *directory);

void tokenize(char *inputString);

char *getLastToken(char *ppromptTermBuf, char *delimiter);

#endif
