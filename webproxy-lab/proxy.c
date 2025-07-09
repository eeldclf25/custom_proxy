#include <stdio.h>
#include <pthread.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3";

void *doit(void* arg);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void parse_uri(char *uri, char *hostname, char *port, char *path);

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

    pthread_t tid;
    Pthread_create(&tid, NULL, doit, &connfd);
    pthread_detach(tid);
  }
}










void *doit(void* arg)
{
  int client_fd = *(int*)arg;
  char temp_buf[MAX_OBJECT_SIZE];

  // ------------------------------클라이언트에서 보낸 request를 분석 및 변환
  rio_t client_rio;
  char client_request[MAXLINE] = {0};
  char method[MAXLINE], host[MAXLINE], port[MAXLINE], path[MAXLINE], version[MAXLINE];

  Rio_readinitb(&client_rio, client_fd);
  memset(temp_buf, 0, sizeof(temp_buf));
  while (strcmp(temp_buf, "\r\n")) {
    Rio_readlineb(&client_rio, temp_buf, MAX_OBJECT_SIZE);

    if (client_request[0] == '\0') {
      char uri[MAXLINE];
      sscanf(temp_buf, "%s %s %s", method, uri, version);
      parse_uri(uri, host, port, path);
      snprintf(client_request + strlen(client_request), sizeof(client_request) - strlen(client_request), "%s %s %s\r\n", method, path, "HTTP/1.0");
    }
    else if (strstr(temp_buf, "User-Agent")) {
      snprintf(client_request + strlen(client_request), sizeof(client_request) - strlen(client_request), "%s\r\n", user_agent_hdr);
    }
    else if (strstr(temp_buf, "Proxy-Connection")) {
      snprintf(client_request + strlen(client_request), sizeof(client_request) - strlen(client_request), "%s\r\n", "Connection: close");
      snprintf(client_request + strlen(client_request), sizeof(client_request) - strlen(client_request), "%s\r\n", "Proxy-Connection: close");
    }
    else {
      snprintf(client_request + strlen(client_request), sizeof(client_request) - strlen(client_request), "%s", temp_buf);
    }
  }
  // ------------------------------------------------------------------------------

  printf("Reformatted request header:\n\n");
  printf("%s\n\n", client_request);

  if (strcasecmp(method, "GET")) {
    clienterror(client_fd, method, "501", "Not implemented", "Tiny does not implement this method");
    pthread_exit(NULL);
  }

  // ------------------------------변환한 request를 서버에 전송 및 응답 데이터를 클라이언트에 전송
  rio_t server_rio;
  char server_header[MAXLINE] = {0};
  char *server_body;
  int content_length;

  int serverfd = Open_clientfd(host, port);
  Rio_readinitb(&server_rio, serverfd);
  Rio_writen(serverfd, client_request, strlen(client_request));
  memset(temp_buf, 0, sizeof(temp_buf));
  while (strcmp(temp_buf, "\r\n")) {
    Rio_readlineb(&server_rio, temp_buf, MAXLINE);

    if (strstr(temp_buf, "Content-length")) {
      content_length = atoi(strchr(temp_buf, ':') + 1);
      snprintf(server_header + strlen(server_header), sizeof(server_header) - strlen(server_header), "%s", temp_buf);
    }
    else {
      snprintf(server_header + strlen(server_header), sizeof(server_header) - strlen(server_header), "%s", temp_buf);
    }
  }
  
  server_body = malloc(content_length);
  Rio_readnb(&server_rio, server_body, content_length);
  Rio_writen(client_fd, server_header, strlen(server_header));
  Rio_writen(client_fd, server_body, content_length);
  free(server_body);
  // ----------------------------------------------------------------------------------------------
  
  printf("Server response header:\n\n");
  printf("%s\n\n", server_header);

  Close(serverfd);
  Close(client_fd);
  
  pthread_exit(NULL);
}










void parse_uri(char *uri, char *hostname, char *port, char *path)
{
  // host_name의 시작 위치 포인터: '//'가 있으면 //뒤(ptr+2)부터, 없으면 uri 처음부터
  char *hostname_ptr = strstr(uri, "//") ? strstr(uri, "//") + 2 : uri;
  char *port_ptr = strchr(hostname_ptr, ':'); // port 시작 위치 (없으면 NULL)
  char *path_ptr = strchr(hostname_ptr, '/'); // path 시작 위치 (없으면 NULL)
  strcpy(path, path_ptr);

  if (port_ptr)
  {
    strncpy(port, port_ptr + 1, path_ptr - port_ptr - 1); 
    strncpy(hostname, hostname_ptr, port_ptr - hostname_ptr);
  }
  else
  {
    strcpy(port, "8000");
    strncpy(hostname, hostname_ptr, path_ptr - hostname_ptr);
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