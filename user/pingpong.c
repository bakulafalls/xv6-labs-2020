#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char const *argv[])
{
  if (argc != 1) {  // 参数错误
    fprintf(2, "usage: pingpong\n");
    exit(1);
  }

  int pid, n = 0;  // n为退出错误flag
  int fds1[2];  // parent->child
  int fds2[2];  // child->parent
  char buf = 'Z';  // 用于传送的字节

  // create two pipes, with four FDs in fds1[0], fds1[1], fds2[0], fds2[1]
  pipe(fds1);
  pipe(fds2);

  pid = fork();
  if (pid < 0) {
    fprintf(2, "fork() error!\n");
    // 错误处理
    close(fds1[1]);
    close(fds2[0]); 
    close(fds1[0]);
    close(fds2[1]);
    n = 1;
  }
  else if (pid == 0) {  // child proc 
    // 关闭子进程中不用的管道口
    close(fds1[1]);
    close(fds2[0]); 
    // if read buf, print
    if (read(fds1[0], &buf, sizeof(buf)) != sizeof(char)) {
      fprintf(2, "child read error!\n");
      n = 1;
    }
    else {
      fprintf(2, "%d: received ping\n", getpid());
    }
    close(fds1[0]);
    // send buf to parent through pipe2
    if (write(fds2[1], &buf, sizeof(buf)) != sizeof(char)) {
      fprintf(2, "child write error!\n");
      n = 1;
    }
    close(fds2[1]);
    // 退出子进程
    exit(n);
  }
  else {  // parent proc
    // 关闭父进程中不用的管道口
    close(fds1[0]);
    close(fds2[1]);
    // 通过pipe1向Child发送buf
    if (write(fds1[1], &buf, sizeof(buf)) != sizeof(char)) {
      fprintf(2, "parent write error!\n");
      n = 1;
    }
    close(fds1[1]);
    // if read buf, print
    if (read(fds2[0], &buf, sizeof(buf)) != sizeof(char)) {
      fprintf(2, "parent read error!\n");
      n = 1;
    }
    else {
      fprintf(2, "%d: received pong\n", getpid());
    }
    close(fds2[0]);

    // 等待子进程退出
    wait(&n);
  }

  exit(n);
}
