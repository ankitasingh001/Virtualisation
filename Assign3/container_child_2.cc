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
#include <string.h>
#include <fstream>
// #define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sched.h>

#define atoa(x) #x

int TRY(int status, const char *msg) {
 if(status == -1) {  
    perror(msg); 
    exit(EXIT_FAILURE);
 }
 return status;
}

void write_rule(const char* path, const char* value) {
  int fp = open(path, O_WRONLY | O_APPEND );
  write(fp, value, strlen(value));
  close(fp);
} 

#define CGROUP_FOLDER "/sys/fs/cgroup/pids/container/" 
#define MEMORY_CGROUP "/sys/fs/cgroup/memory/container/"
#define CPU_CGROUP "/sys/fs/cgroup/cpu/container/"
#define concat(a,b) (a"" b)
void limitProcessCreation() {
  mkdir( CGROUP_FOLDER, S_IRUSR | S_IWUSR);  // Read & Write
  mkdir( MEMORY_CGROUP, S_IRUSR | S_IWUSR); 
  mkdir( CPU_CGROUP, S_IRUSR | S_IWUSR);  // Read & Write
  const char* pid  = std::to_string(getpid()).c_str();

  write_rule(concat(CGROUP_FOLDER, "pids.max"), "10"); 
  write_rule(concat(CGROUP_FOLDER, "notify_on_release"), "1"); 
  write_rule(concat(CGROUP_FOLDER, "cgroup.procs"), pid);

    /* uncomment below lines to show memory limits */

//   write_rule(concat(CPU_CGROUP, "cpu.cfs_quota_us"), "4000");
//   write_rule(concat(CPU_CGROUP, "cpu.cfs_period_us"), "10000");
//   write_rule(concat(CPU_CGROUP, "cpu.shares"), "320");
//     write_rule(concat(CPU_CGROUP, "notify_on_release"), "1"); 
//  write_rule(concat(CPU_CGROUP, "cgroup.procs"), pid);
//   write_rule(concat(MEMORY_CGROUP, "memory.limit_in_bytes"),"10000");
//  write_rule(concat(MEMORY_CGROUP, "memory.swappiness"),"0");
//   write_rule(concat(MEMORY_CGROUP, "notify_on_release"), "1"); 
//  write_rule(concat(MEMORY_CGROUP, "cgroup.procs"), pid);

}

char* stack_memory() {
  const int stackSize = 65536;
  auto *stack = new (std::nothrow) char[stackSize];

  if (stack == nullptr) { 
    printf("Cannot allocate memory \n");
    exit(EXIT_FAILURE);
  }  

  return stack+stackSize;  //move the pointer to the end of the array because the stack grows backward. 
}

void setHostName(std::string hostname) {
  sethostname(hostname.c_str(), hostname.size());
}

void setup_variables() {
  clearenv();
  setenv("TERM", "xterm-256color", 0);
  setenv("PATH", "/bin/:/sbin/:usr/bin:/usr/sbin", 0);
}

template <typename... P> 
int run(P... params) {
  char *args[] = {(char *)params..., (char *)0};
  
  execvp(args[0], args); 
  perror("execvp"); 
  return 0;
}

int run2(const char *name) {
  char *_args[] = {(char *)name, (char *)0 };
  execvp(name, _args);
}

void setup_root(const char* folder){
  chroot(folder);
  chdir("/");
}

template <typename Function>
void clone_process(Function&& function, int flags){
 auto pid = TRY( clone(function, stack_memory(), flags, 0), "clone" );
  printf("child pid as seen by parent: %d\n", pid);
  // add route to parent
  system("ip -n vnet0 route add 10.0.2.2 dev vethp");
  printf("route added from parent to child 2!");
 wait(nullptr); 
} 

#define lambda(fn_body) [](void *args) ->int { fn_body; };
 
int jail(void *args) {

  limitProcessCreation();


  printf("\n\n");
  setHostName("my-container_2");
  setup_variables();

  setup_root("./rootfs_2");

  mount("proc", "/proc", "proc", 0, 0); 
  printf("HERE IN CHILD");


  auto runnable = lambda(run("/bin/sh"))
  // printf("child pid: %d\n", getpid());
  // printf("New `net` Namespace:\n");
  // system("ip link add veth1 type veth peer name vethp");
  // system("ip link list");
  system("ip netns add vnet2");

  system("ip link add veth2 type veth");

  system("ip link set veth2 netns vnet2");
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
  printf("route added from child to parent!");
  /** For container to container **/
  system("ip route add 10.0.2.1/24 dev veth2");
  printf("route added from child to child!");
  clone_process(runnable, SIGCHLD);

  // system("ip netns exec vnet1 link set veth1 up");
  // system("ip netns exec vnet1 link set lo up");
  // printf("\n Confirming that interfaces are up... \n");
  // system("ip netns exec vnet1 addr show");
  umount("/proc");
  return EXIT_SUCCESS;
}

int main(int argc, char** argv) {


  printf("parent pid: %d\n", getpid());

  printf("Original `net` Namespace:\n");

  system("ip link add vethp type veth peer name veth2");
  //system("ip link add veth2 type veth");

  // system("ip link show vethp");
  // system("ip link show veth1");
  system("ip netns add vnet0");
  system("ip link set vethp netns vnet0");
  system("ip -n vnet0 addr add 10.0.1.0/24 dev vethp");
  system("ip -n vnet0 link set vethp up");
  system("ip -n vnet0 link set lo up");
  printf("\n Confirming that interfaces are up... \n");
  system("ip -n vnet0 addr show");
  // system("ip addr add 10.1.1.1/24 dev parent");
  // system("ip link set dev parent up");
  //system("ip link");
  printf("\n\n");

  clone_process(jail,  CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD );

  return EXIT_SUCCESS;
}
