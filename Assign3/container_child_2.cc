#include <iostream>
#include <sched.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <fstream>
// #define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sched.h>

#define PID_CGROUP "/sys/fs/cgroup/pids/container/" 
#define MEMORY_CGROUP "/sys/fs/cgroup/memory/container/"
#define CPU_CGROUP "/sys/fs/cgroup/cpu/container/"
#define concat(a,b) (a"" b)
#define atoa(x) #x
#define lambda(fn_body) [](void *args) ->int { fn_body; };

/**
 *  Write to Cgroup files
 * */
void make_cgroup_rule(const char* cgroup_path, const char* val) {
  int fileptr = open(cgroup_path, O_WRONLY | O_APPEND );
  write(fileptr, val, strlen(val));
  close(fileptr);
} 

/**
 *  Setting Cgroup parameters
 * */
void set_cgroup_params() {
  /**
   * Creating Cgroups to limit processes/memory/CPU
   * */
  mkdir( PID_CGROUP, S_IRUSR | S_IWUSR);  
  mkdir( MEMORY_CGROUP, S_IRUSR | S_IWUSR); 
  mkdir( CPU_CGROUP, S_IRUSR | S_IWUSR);  

  const char* pid  = std::to_string(getpid()).c_str();

  make_cgroup_rule(concat(PID_CGROUP, "pids.max"), "10"); 
  make_cgroup_rule(concat(PID_CGROUP, "notify_on_release"), "1"); 
  make_cgroup_rule(concat(PID_CGROUP, "cgroup.procs"), pid);

  /* uncomment below lines to show memory limits */

//   make_cgroup_rule(concat(CPU_CGROUP, "cpu.cfs_quota_us"), "4000");
//   make_cgroup_rule(concat(CPU_CGROUP, "cpu.cfs_period_us"), "10000");
//   make_cgroup_rule(concat(CPU_CGROUP, "cpu.shares"), "320");
//   make_cgroup_rule(concat(CPU_CGROUP, "notify_on_release"), "1"); 
//   make_cgroup_rule(concat(CPU_CGROUP, "cgroup.procs"), pid);
//   make_cgroup_rule(concat(MEMORY_CGROUP, "memory.limit_in_bytes"),"10000");
//   make_cgroup_rule(concat(MEMORY_CGROUP, "memory.swappiness"),"0");
//   make_cgroup_rule(concat(MEMORY_CGROUP, "notify_on_release"), "1"); 
//   make_cgroup_rule(concat(MEMORY_CGROUP, "cgroup.procs"), pid);

}

char* stack_size() 
{
  const int Size = 98765;
  auto *mystack = new (std::nothrow) char[Size];
  if (mystack == nullptr) { 
    printf("Memory allocation failed\n");
    exit(EXIT_FAILURE);
  }  
  return mystack+Size; //Stack size grows backward
}


/**
 * Run shell
 * */
int run(const char *n) {
  char *_args[] = {(char *)n, (char *)0 };
  execvp(n, _args);
}


int see_status(int s, const char *msg) 
{
 if(s == -1) 
 {  
    perror(msg); 
    exit(EXIT_FAILURE);
 }
 return s;
}




template <typename Function>
void call_clone(Function&& function, int flags)
{
 /**
  * Call clone and set child_pid to process id of the cloned process
  * */
  auto child_pid = see_status( clone(function, stack_size(), flags, 0), "clone function call failed" );
  // if(child_pid == -1) {  
  //     perror("Clone function call failed"); 
  //     exit(EXIT_FAILURE);
  // }
  printf("child pid as seen by parent: %d\n", child_pid);

  // add route to parent
  system("ip -n vnet0 route add 10.0.2.2 dev vethp");
  printf("route added from parent to child 2!\n");

  wait(nullptr); 
} 

 
int child_process(void *args) {

  set_cgroup_params();

  printf("\n\n");

  /**
   * Chnage HOSTNAME to change default hostname
   * */
  std::string HOSTNAME ="Ankita-container_2";
  sethostname(HOSTNAME.c_str(), HOSTNAME.size());

  /**
   * Setting up environment variables 
   * */
  clearenv();
  setenv("TERM", "xterm-256color", 0);
  setenv("PATH", "/bin/:/sbin/:usr/bin:/usr/sbin", 0);

  /**
   * Change this as per the name of the root file directory set up
   * */
  chroot("./rootfs_2");
  chdir("/");

  mount("proc", "/proc", "proc", 0, 0); 
  printf("HERE IN CHILD");


  auto runnable = lambda(run("/bin/sh"))

  printf("\n Child Namespace:\n");

  system("ip netns add vnet2");

  system("ip link add veth2 type veth");

  system("ip link set veth2 netns vnet2");
  //set network namespace path
  const char *path = "/var/run/netns/vnet2";

  int fd = open(path,O_RDONLY);
  if(fd ==-1)
  {
    printf("error in file descriptor of namespace");
    exit(EXIT_FAILURE);
  }
  if(setns(fd,CLONE_NEWNET)==-1)
  {
    printf("error while setting network namespace");
    exit(EXIT_FAILURE);
  }
  system("ip addr add 10.0.2.2/24 dev veth2");
  system("ip link set veth2 up");
  system("ip link set lo up");
  printf("\n Confirming that interfaces are up... \n");
  system("ip addr show");
  
  system("ip route add 10.0.1.0/24 dev veth2");
  printf("\nroute added from child to parent!");
  /** For container to container **/
  system("ip route add 10.0.2.1/24 dev veth2");
  printf("\nroute added from child to child!");
  call_clone(runnable, SIGCHLD);

  umount("/proc");
  return EXIT_SUCCESS;
}

int main(int argc, char** argv) {


  printf("Parent Pid -> %d ", getpid());

  printf("\nParent/default Namespace:\n");


  system("ip link add vethp type veth peer name veth2");

  // system("ip link show vethp");
  // system("ip link show veth1");
  system("ip netns add vnet0");
  system("ip link set vethp netns vnet0");
  system("ip -n vnet0 addr add 10.0.1.0/24 dev vethp");
  system("ip -n vnet0 link set vethp up");
  system("ip -n vnet0 link set lo up");
  printf("\n Confirming that interfaces are up... \n");
  system("ip -n vnet0 addr show");

  printf("\n");

  call_clone(child_process,  CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD );

  return EXIT_SUCCESS;
}
