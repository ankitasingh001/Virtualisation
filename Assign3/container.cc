#include <iostream>
#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <fstream>
// #define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sched.h>

#define atoa(x) #x


int run_shell(const char *param) {

  char *_args[] = {(char *)param, (char *)0 };
  execvp(param, _args);
}

/**
 * Run a shell in the parent namespace
 * */
int main(int argc, char** argv)
 {
  printf("Parent Pid -> %d ", getpid());

  printf("\nDefault/Parent Namespace:\n");
  /**
   * Type the below command on shell to show that network interfaces are created 
   * Or run parent shell only after running container_child
   * */
  //system("ip -n vnet0 addr show");

  /**
   * To run client type the following
   *  ip netns exec vnet0 python Client.py low
   * */
  run_shell("/bin/sh");

  return EXIT_SUCCESS;
}
