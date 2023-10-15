#include <stdio.h>

FILE* openFile(char* fileName, char* mode);


int closeFile(FILE* fp);


unsigned int findFileSize(FILE* fp);