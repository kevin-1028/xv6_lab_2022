#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"


int 
main(int argc, char *argv[]){
    int time;
    if(argc < 2){
        printf("error message");
        exit(1);
    }

    for(int i = 1; i < argc; i++){
        time = atoi(argv[i]);
        sleep(time);
    }
    exit(0);
}
