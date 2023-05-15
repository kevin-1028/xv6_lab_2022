#include "kernel/types.h"
#include "user/user.h"

void prime(int p[]){
    int p1[2];
    int buf, n, temp;
    
    close(p[1]);
    n = read(p[0], &buf, 4);
    if(n == 0){
        exit(0);
    }

    if(pipe(p1) == -1){
        printf("pipe create error");
        exit(1);
    }

    if(fork() == 0){
        //child
        close(p1[1]);
        close(p[0]);
        prime(p1);
    }else{
        close(p1[0]);
        if(n != 0){
            printf("prime %d\n", buf);
        }
       while(n){
            n = read(p[0], &temp, 4);
            if(temp % buf != 0){
                write(p1[1], &temp, 4);
            }
        }
        close(p[0]);
        close(p1[1]);
        wait(0);
    }
    exit(0);
}

int main(int argc, char *argv[]){
    int p[2];

    if(pipe(p) == -1){
        printf("pipe create error");
        exit(1);
    }

    if(fork() == 0){
        //child
        //close(p[1]);
        prime(p);
    }else{
        //parent
        close(p[0]);

        for(int i = 2; i <= 35; i++){
            write(p[1], &i, 4);
        }
        close(p[1]);
        wait(0);
    }
    
    exit(0);

}
