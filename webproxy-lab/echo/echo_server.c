#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>


#define LISTENQ 1024 // listen의 대기 큐 사이즈 설정
#define MAXLINE 8192


// listening socket을 만들기 위한 함수
int open_listenfd(char *port)
{
    struct addrinfo hints, *listp, *p;  // getaddrinfo으로 출력 받을 변수 및 필터링을 위한 hints 변수
    int listenfd, rc, optval=1;

    memset(&hints, 0, sizeof(struct addrinfo)); // 필터링을 하기 위한 hints 변수 초기화
    hints.ai_socktype = SOCK_STREAM;             // 소켓이 데이터를 주고 받는 방식을 정의하는데, SOCK_STREAM은 TCP을 뜻함
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; // 내 컴퓨터에 들어올 수 있는 모든 주소인 와일드 주소(0, 0, 0, 0)를 반환하며 | 로컬 호스트의 IPv4나 IPv6에 따르게 설정됨
    hints.ai_flags |= AI_NUMERICSERV;            // 입력하려는 포트는 숫자로 되어 있다는 것을 알리는 필터링
    if ((rc = getaddrinfo(NULL, port, &hints, &listp)) != 0) {  // getaddrinfo 함수의 호스트에 NULL을 넣었다는 것은, 결국 서버 소켓을 만들때 사용할 정보를 요청하는 것임
        fprintf(stderr, "getaddrinfo failed (port %s): %s\n", port, gai_strerror(rc));
        return -2;
    }

    // getaddrinfo으로 반환된 addrinfo타입의 연결 리스트를 담은 listp를 순회
    for (p = listp; p; p = p->ai_next) {
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)  // p를 이용하여 파일 디스크럽터인 socket을 생성
            continue; // 실패하면 continue

        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)); // 포트 사용종료 직후 바로 다시 실행하면, 이전에 사용했던 포트가 아직 운영체제에 의해 완전히 해제되지 않은 상태일 수 있는데, 이 상태를 업데이트 하는 함수

        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) // 해당 소켓을 해당 addr주소로 할당하게 하는 함수
            break;
        if (close(listenfd) < 0) { // 바인드를 실패했으니까 해당 소켓을 닫는 함수
            fprintf(stderr, "open_listenfd close failed: %s\n", strerror(errno));
            return -1;
        }
    }

    // 해당 addrinfo 리스트를 free
    freeaddrinfo(listp);
    if (!p) // p가 널이면 next로 계속 갔는데도 결국 NULL까지 가고 소켓 bind에 실패했다는 거니까 -1 return
        return -1;

    if (listen(listenfd, LISTENQ) < 0) {    // 해당 소켓을 연결요쳥을 받을 수 있는 수신 소켓으로 변경
        close(listenfd);
	    return -1;
    }

    return listenfd;
}

#define RIO_BUFSIZE 8192
typedef struct {
    int rio_fd;                /* Descriptor for this internal buf */
    int rio_cnt;               /* Unread bytes in internal buf */
    char *rio_bufptr;          /* Next unread byte in internal buf */
    char rio_buf[RIO_BUFSIZE]; /* Internal buffer */
} rio_t;

void rio_readinitb(rio_t *rp, int fd) 
{
    rp->rio_fd = fd;  
    rp->rio_cnt = 0;  
    rp->rio_bufptr = rp->rio_buf;
}

static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;

    while (rp->rio_cnt <= 0) {  /* Refill if buf is empty */
	rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, 
			   sizeof(rp->rio_buf));
	if (rp->rio_cnt < 0) {
	    if (errno != EINTR) /* Interrupted by sig handler return */
		return -1;
	}
	else if (rp->rio_cnt == 0)  /* EOF */
	    return 0;
	else 
	    rp->rio_bufptr = rp->rio_buf; /* Reset buffer ptr */
    }

    /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
    cnt = n;          
    if (rp->rio_cnt < n)   
	cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}

ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    int n, rc;
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++) { 
        if ((rc = rio_read(rp, &c, 1)) == 1) {
	    *bufp++ = c;
	    if (c == '\n') {
                n++;
     		break;
            }
	} else if (rc == 0) {
	    if (n == 1)
		return 0; /* EOF, no data read */
	    else
		break;    /* EOF, some data was read */
	} else
	    return -1;	  /* Error */
    }
    *bufp = 0;
    return n-1;
}

ssize_t rio_writen(int fd, void *usrbuf, size_t n) 
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;

    while (nleft > 0) {
	if ((nwritten = write(fd, bufp, nleft)) <= 0) {
	    if (errno == EINTR)  /* Interrupted by sig handler return */
		nwritten = 0;    /* and call write() again */
	    else
		return -1;       /* errno set by write() */
	}
	nleft -= nwritten;
	bufp += nwritten;
    }
    return n;
}

void unix_error(char *msg) /* Unix-style error */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}

void Rio_writen(int fd, void *usrbuf, size_t n) 
{
    if (rio_writen(fd, usrbuf, n) != n)
	unix_error("Rio_writen error");
}

void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    rio_readinitb(&rio, connfd);
    while ((n = rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("server received %d bytes\n", (int)n);
        Rio_writen(connfd, buf, n);
    }
}


int main(int argc, char **argv)
{
    if (argc != 2) { // main 인자 체크
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    else {
        printf("server open. port : %s\n", argv[1]);
    }

    int listenfd = open_listenfd(argv[1]);  // main 인자로 들어온 수치로 연결 요청을 받을 수 있는 수신 대기 소켓을 생성

    while (true) {
        
        struct sockaddr_storage clientaddr; // accept 함수로 연결된 클라이언트를 받을 주소받을 예정인데 어떤 주소 크기가 올지 모르니까, 일단 sockaddr_storage 이라는 크게 설계된 구조체를 사용
        socklen_t clientlen = sizeof(struct sockaddr_storage);    // 해당 sockaddr_storage 구조체의 크기
        int connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen); // accept로 해당 리스닝 소켓인 listenfd의 대기열을 대기하며, 대기열에 특정 연결이 올 경우 해당 클라이언트의 정보를 clientaddr, clientlen를 저장, 이후 연결된 파일 디스크립터를 connfd에 저장
        
        char client_hostname[MAXLINE];
        char client_port[MAXLINE];
        getnameinfo((struct sockaddr *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0); // accept을 통해 받은 clientaddr의 안에 있는 정보를 문자열로 변환해주는 함수
        printf("Connected to (%s, %s)\n", client_hostname, client_port);

        echo(connfd);
        close(connfd);
        printf("End to (%s, %s)\n", client_hostname, client_port);
    }

    exit(0);
}