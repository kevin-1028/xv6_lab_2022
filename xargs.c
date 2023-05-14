/*#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"
#include "kernel/stat.h"


int main(int argc, char *argv[]){
    char buf[512];
    char *xargv[MAXARG];
    int i;
    int len;
    if(argc < 2 || argc - 1 > MAXARG){
        fprintf(2, "number of parameter is too much or less\n");
        exit(1);
    } 
    
    memset(buf, 0, sizeof(buf));
    for(i = 1; i < argc; i++){
        xargv[i - 1] = argv[i];
    }
    for(; i < MAXARG; i++)
        xargv[i] = 0;

    while(1){
        i = 0;
        while(1){
            len = read(0, &buf[i], 1);
            if(len <= 0 || buf[i] == '\n') break;
            i++;
        }
        if(i == 0) break;
        buf[i] = 0;
        xargv[argc - 1] = buf;
        if(fork() == 0){
            exec(xargv[0], xargv);
            exit(0);
        }else{
            wait(0);
        }
    }
    exit(0);
}*/

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

void xargs(char *argv[]) {
    int pid, status;
    if ((pid=fork()) == 0) {
        exec(argv[0], argv);
        exit(1);
    }
    wait(&status);
    return;
}

void main(int argc, char *argv[]) {

    int i, j;
    char c, buf[512], *xargv[MAXARG];
    if (argc < 2 || argc - 1 > MAXARG) {
        fprintf(2, "usage: xargs <cmd> {args}, # of args should be less than 32\n");
        exit(1);
    }

    memset(buf, 0, sizeof(buf));
    for (i = 1; i < argc; i++)
        xargv[i - 1] = argv[i];
    for (; i < MAXARG ; i++)
        xargv[i] = 0;

    j = 0;
    while (read(0, &c, 1) > 0) {
        if (c != '\n')
            buf[j++] = c;
        else {
            if (j != 0) {
                buf[j] = '\0';
                xargv[argc - 1] = buf;
                xargs(xargv);
                j = 0;
            }
        }
    }
    if (j != 0) {
        buf[j] = '\0';
        xargv[argc - 1] = buf;
        xargs(xargv);
    }
    exit(0);
}





