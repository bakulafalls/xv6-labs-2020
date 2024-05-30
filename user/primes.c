#include "kernel/types.h"
#include "user/user.h"

/**
 *@brief 读取上一个管道的第一个数据并打印
 *@param pfirst 指向储存第一个数据的指针
*/
int
lpipe_first_data(int lpipe[2], int *pfirst)
{
  if (read(lpipe[0], pfirst, sizeof(int)) == sizeof(int)) {
    fprintf(2, "prime %d\n", *pfirst);
    return 0;
  }
  return -1;
}


/**
 *@brief 将lpipe中的数据传入p中，剔除掉不能被 first_number整除的数据
 *@param lpipe 上一个管道
 *@param rpipe 新的管道
 *@param pfirst 指向储存第一个数据的指针
*/
void
transmit_data(int lpipe[2], int rpipe[2], int *pfirst)
{
  int data;  // 存放读取的lpipe数据
  while (read(lpipe[0], &data, sizeof(int)) == sizeof(int))
  {
    // 将无法整除的数据传入rpipe
    if (data % *pfirst) {
      write(rpipe[1], &data, sizeof(int));
    }
  }
  close(lpipe[0]);
  close(rpipe[1]);
}


/**
 *@brief 寻找素数 
 *@param lpipe: 上一个管道的管道符
*/
__attribute__((noreturn))
void
find_primes(int lpipe[2])
{
  // 关闭上一个管道的写入端，已经不需要了
  close(lpipe[1]);
  int first_number;
  if (lpipe_first_data(lpipe, &first_number) == 0)  // 打印上一个管道得到的素数
  {
    int p[2];
    pipe(p);  // 新建管道作为当前管道
    //  将lpipe中的数据传入p中，剔除掉不能被 first_number整除的数据
    transmit_data(lpipe, p, &first_number);
    if (fork() == 0) {  //  新建子进程处理新的管道
      close(p[1]);
      find_primes(p);  // 递归
    }
    else {
      close(p[0]);
      close(p[1]);
      wait(0);  // 最后一个孙进程结束前，所有之前的进程都等待
    }
  }
  exit(0);
}


int
main(int argc, char const *argv[])
{
  if (argc != 1) {  // 参数错误
    fprintf(2, "usage: primes\n");
    exit(1);
  }

  int pid;
  int fds[2];
  pipe(fds);

  for (int i=2; i <= 35; i++)  // 写入2-35到第一个管道中
  {
    write(fds[1], &i, sizeof(int));
  }

  pid = fork();
  if (pid == 0) {  // child
    // 寻找素数
    find_primes(fds);
  }
  else {  // oldest parent, do nothing but wait
    close(fds[0]);
    close(fds[1]);
    wait(0);
  }
  exit(0);
}
