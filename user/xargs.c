#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define MAXSZ 512

// 当前字符状态定义
enum state {
  S_WAIT,           // 等待输入（初始状态、空格）
  S_ARG,            // 当前为参数
  S_ARG_END,        // 参数结束
  S_ARG_LINE_END,   // 左侧有参数的换行，e.g.: "arg\n"
  S_LINE_END,       // 左侧有空格的换行，e.g.: "arg \n"
  S_END             // 结束
};

// 字符类型定义
enum char_type {
  C_SPC,  // 空格
  C_CHR,  // 普通字符
  C_LNEND // 换行符
};


/**
 *@brief 获取字符类型
 *@param c 要判定的字符
*/
enum char_type get_char_type(char c)
{
  switch (c) {
    case ' ':
      return C_SPC;
    case '\n':
      return C_LNEND;
    default:
      return C_CHR;
  }
}


/**
 *@brief 根据当前字符类型转换当前状态
 *@param last_state 判定前的状态
 *@param cur_char 当前字符种类
*/
enum state transform_state(enum state last_state, enum char_type cur_char)
{
  switch (last_state) {
    case S_WAIT:
      if (cur_char == C_SPC)    return S_WAIT; // 继续等待
      if (cur_char == C_LNEND)  return S_LINE_END;
      if (cur_char == C_CHR)    return S_ARG;
      break;
    case S_ARG:
      if (cur_char == C_SPC)    return S_ARG_END;  // 参数结束
      if (cur_char == C_LNEND)  return S_ARG_LINE_END;
      if (cur_char == C_CHR)    return S_ARG;  
      break;
    case S_ARG_END:  
    case S_ARG_LINE_END:
    case S_LINE_END:  // 换行后情况都一样
      if (cur_char == C_SPC)    return S_WAIT;  
      if (cur_char == C_LNEND)  return S_LINE_END;  // 连换两行
      if (cur_char == C_CHR)    return S_ARG;  
    default:  // S_END
      break;
  }
  return S_END;
}


int
main(int argc, char *argv[])  
{
  if (argc > MAXARG)  { // 参数错误
    fprintf(2, "Too many arguments!\n");
    exit(1);
  }

  enum state st = S_WAIT;  // 用于表示状态
  char lines[MAXSZ];  // 初始化内存用于读取命令参数
  char *p = lines;  // 初始化指针指向用于储存命令行的缓存
  char *new_argv[MAXARG] = {0};  // 初始化用于存储参数的指针数组
  int arg_start = 0;  // 参数起点
  int arg_end = 0;  // 参数终点
  int arg_cur = argc-1;  // 当前参数索引, 前面已经有argc-1个参数被记录了，如实例1中的"echo","bye",为argv[1:2]

  // 存储管道写入端里的原有的命令参数,如实例1中的"argx","echo","bye"，argc=3, 我们只储存"echo","bye"
  for (int i = 1; i < argc; ++i) {
    new_argv[i - 1] = argv[i];
  }

  // 对每一行命令参数循环, 每个while读取一个字符
  while (st != S_END ) {
    if (read(0, p, sizeof(char)) != sizeof(char)) {  // 读取到的命令为空，设置状态为结束
      st = S_END;
    }
    else {
      st = transform_state(st, get_char_type(*p));
      ++arg_end;  // 新读取了一个字符，需要更新当前参数结束位置
    }

    switch (st) {
      case S_WAIT:  // 忽略掉当前的空格，右移参数起始指针
        ++arg_start;
        break; 
      case S_ARG_END:  // 在new_argv中储存当前参数地址
        new_argv[arg_cur++] = &lines[arg_start];
        arg_start = arg_end;  // 移动参数起点到下一个参数位置
        *p = '\0';  // 在缓存中参数结尾添加字符串结束符
        break;
      case S_ARG_LINE_END:  // 先将参数地址存入newargv,然后和S_LINE_END一样执行命令
        new_argv[arg_cur++] = &lines[arg_start];
      case S_LINE_END:  // 为当前行执行命令
        arg_start = arg_end;
        *p = '\0';
        new_argv[arg_cur] = 0;  // 确保参数列表以NULL结束
        if (fork() == 0) {
          exec(argv[1], new_argv);  // arg[0]为xargs
          exit(0);  // 确保子进程在exec失败时退出 
        }
        arg_cur = argc - 1;  // 重置位置
        // 换行，清空参数
        for (int i = arg_cur; i < MAXARG; ++i)
          new_argv[i] = 0;
        wait(0);
        break;
      default:
        break;
    }

    p++;  // 读取下一个字符
  }

  exit(0);
}
