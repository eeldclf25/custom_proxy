/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);
    Close(connfd);
    printf("close connfd\n");
  }
}

void doit(int fd)
{
  rio_t rio;
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], filename[MAXLINE], cgiarge[MAXLINE];
  Rio_readinitb(&rio, fd);
  if (!(Rio_readlineb(&rio, buf, MAXLINE))) {
    printf("no-data socket\n");
    return;
  }
  // Rio_readlineb(&rio, buf, MAXLINE);
  sscanf(buf, "%s %s %s", method, uri, version);
  printf("Request headers : \n");
  printf("%s\n", buf);
  
  if (strcasecmp(method, "GET")) { // 해당 문자가 (대소문자 제외)정확하면 0, 이외는 양수나 음수
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  read_requesthdrs(&rio);
  is_static = parse_uri(uri, filename, cgiarge);  // url 부분 filename, cgiarge에 파싱하고 정적인지 동적인지 is_static에 저장

  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, method, "404", "Not found", "Tiny couldn't this file");
    return;
  }

  if (is_static) {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, method, "403", "Forbidden", "Tiny couldn't read this file");
      return;
    }
    else {
      serve_static(fd, filename, sbuf.st_size);
    }
  }
  else {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, method, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    else {
      serve_dynamic(fd, filename, cgiarge);
    }
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=\"ffffff\">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);  
  printf("%s", buf);

  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }

  return; // Rio_readlineb을 하면서 결국 \r\n가 없어질때까지 rio 읽기 위치 이동
}

int parse_uri(char *uri, char *filename, char *cgiarge)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")) {  // uri에 cgi-bin 이 없는 경우 (home.html 이라는 정적 컨텐츠만)
    strcpy(cgiarge, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri) - 1] == '/')
      strcat(filename, "home.html");  // filename = ./home.html
    return 1;
  }
  else {  // 이때는 동적 컨텐츠을 위한 준비
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiarge, ptr + 1); // cgiarge = 1500&213
      *ptr = '\0';
    }
    else {
      strcpy(cgiarge, ""); // cgiarge = ""
    }
    strcpy(filename, ".");
    strcat(filename, uri);  // filename = ./cgi-bin/adder
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize)
{
  char *srcp, filetype[MAXLINE], buf[MAXBUF];
  int srcfd;

  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);  
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);  // 헤더 정보 보내기

  srcfd = Open(filename, O_RDONLY, 0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize); // 그냥 파일 그대로 전송
  Munmap(srcp, filesize);
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) {
    setenv("QUERY_STRING", cgiargs, 1); // execve 하기 전에 환경변수 설정
    Dup2(fd, STDOUT_FILENO);  // asd
    Execve(filename, emptylist, environ); // 실행하려는 파일, main에 들어가는 arge 배열, 환경변수 배열(NULL이면 실행 전 환경변수 그대로 진행)
  }

  Wait(NULL);
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mpg"))
    strcpy(filetype, "video/mpg");
  else
    strcpy(filetype, "text/plain");
} 