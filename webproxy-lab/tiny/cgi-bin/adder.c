/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
//#include "csapp.h"
#include "../csapp.h"

int main(void)
{
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  if ((buf = getenv("QUERY_STRING")) != NULL) // adder로 execve 전환되기 전에 입력한 환경변수 QUERY_STRING를 읽음
  {
    p = strchr(buf, '&');
    *p = '\0';
    strcpy(arg1, buf);
    strcpy(arg2, p + 1);
    n1 = atoi(strchr(arg1, '=') + 1);
    n2 = atoi(strchr(arg2, '=') + 1);
  }

  // sprintf 함수는 해당 버퍼(content)에 문자열을 저장
  sprintf(content, "QUERY_STRING=%s\r\n<p>", buf);
  sprintf(content + strlen(content), "Welcome to add.com: ");
  sprintf(content + strlen(content), "THE Internet addition portal.\r\n<p>");
  sprintf(content + strlen(content), "The answer is: %d + %d = %d\r\n<p>",
          n1, n2, n1 + n2);
  sprintf(content + strlen(content), "Thanks for visiting!\r\n");

  // 여기서 printf를 하는데, execve 전환되기 전에, 우리는 표준 출력을 클라이언트 소켓으로 바꿧기 때문에 여기서 입력되는 것은 결국 클라이언트 소켓으로 가게 됨
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n");
  printf("\r\n");
  printf("%s", content);
  fflush(stdout); // stdout이 소켓에 연결되어 있어서, 완전버퍼링 형식이 되었기 때문에, 입력한 버퍼를 그냥 바로 전송

  exit(0);
}
/* $end adder */
