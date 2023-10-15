#include "../include/files.h"

FILE* openFile(char* fileName, char* mode){
    FILE* fp;
    fp = fopen(fileName, mode);
    if (fp == NULL)
    {
        return NULL;
    }
    
    return fp;
}


int closeFile(FILE* fp){
    return fclose(fp);
}


unsigned int findFileSize(FILE* fp){ 
    fseek(fp, 0, SEEK_END); 
  
    unsigned int res = (unsigned int)ftell(fp); 
    rewind(fp);

    return res; 
} 