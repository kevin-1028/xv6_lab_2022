#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(int argc, char *argv[]){
    int p_child[2];
    int p_parent[2];

    pipe(p_child);
    pipe(p_parent);
    char c[64];

    if(fork() == 0){
        //child
        int pid = getpid();
        close(p_child[0]);
        close(p_parent[1]);
        read(p_parent[0], c, 4);
        close(p_parent[0]);
        printf("%d: received %s\n", pid, c);
        write(p_child[1], "pong", 4);
        close(p_child[1]);
    }else{
        //parnet
        int pid = getpid();
        close(p_child[1]);
        close(p_parent[0]);
        write(p_parent[1], "ping", 4);
        wait(0);
        read(p_child[0], c, 4);
        printf("%d: received %s\n", pid, c);
    }

    exit(0);

}
