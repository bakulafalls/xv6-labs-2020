#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

/**
 *@brief 寻找指定文件
 *@param path 要搜寻的目录路径  
 *@param filename 要搜寻的文件名
*/
//__attribute__((noreturn))
void
find(char *path, const char *filename)
{
  // 在第一级目录下搜寻
  char buf[512], *p;  // buf用于构建路径
  int fd;
  struct dirent de;  // 目录
  struct stat st;  // 状态信息

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (st.type != T_DIR) {
    fprintf(2, "invaild path! usage: find <dir> <filename>\n");
    close(fd);
    return;
  }

  if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      fprintf(2, "ls: path too long\n");
      return;
    }

  strcpy(buf, path);  // 在 buf 中构建新的路径，为读取目录内容做准备
  p = buf+strlen(buf);
  *p++ = '/';  // p指向最后一个'/'后一个字符 
  while(read(fd, &de, sizeof(de)) == sizeof(de)){  // 遍历当前目录下每一个文件
    if(de.inum == 0)
      continue;
    memmove(p, de.name, DIRSIZ);  // 添加路径名称
    p[DIRSIZ] = 0;  // 在复制的文件名后添加一个字符串结束符
    if(stat(buf, &st) < 0){
      fprintf(2, "ls: cannot stat %s\n", buf);
      continue;
    }

    // 递归， "." 和 ".." 目录除外
    if (st.type == T_DIR && strcmp(p, ".") != 0 && strcmp(p, "..") != 0)
    {
      find(buf, filename);
    }
    else if (strcmp(filename, p) == 0)  // p指向的字符串不为DIR, 而是FILE（忽略DEVICE的情况），进行文件名比对
    printf("%s\n", buf);
  }
}


int
main(int argc, char *argv[])
{
  if (argc != 3) {  // 参数错误
    fprintf(2, "usage: find <dir> <filename>\n");
    exit(1);
  }

  find(argv[1], argv[2]);
  exit(0);
}
