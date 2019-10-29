/* ___________________________________________________________
 * ____________<<<___#_R_V_C_G_E_E_K_S___>>>__________________
 * CREATED BY #RVCGEEKS @PUNE for more rvchavadekar@gmail.com
 *
 * micro HTTP server in 500 lines
 * created on 26.10.2019 4:30am
 * 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>               /* set timeout to read a GET request */
#include <arpa/inet.h>          /* inet_ntoa */
#include <netinet/tcp.h>
#include <sys/sendfile.h>
#include <sys/time.h>
#include <sys/stat.h>


#define LISTENQ  1024  /* second argument to listen() */
#define MAXLINE 1024   /* max length of a line */
#define RIO_BUFSIZE 1024
#define TIMEOUT 3000  /* 10s timeout on idle read */


typedef struct {                /* Persistent state for the robust I/O (rio) */
    int rio_fd;                 /* descriptor for this buf */
    int rio_cnt;                /* unread byte in this buf */
    char *rio_bufptr;           /* next unread byte in this buf */
    char rio_buf[RIO_BUFSIZE];  /* internal buffer */
} rio_t;


typedef struct {
    char filename[512];
    off_t offset;              /* for support Range */
    size_t end;
} http_request;


typedef struct {
    const char *extension;
    const char *mime_type;
} mime_map;


mime_map mime_types[] = {
    {".css", "text/css"},
    {".gif", "image/gif"},
    {".htm", "text/html"},
    {".html", "text/html"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"},
    {".ico", "image/x-icon"},
    {".js", "application/javascript"},
    {".pdf", "application/pdf"},
    {".mp4", "video/mp4"},
    {".mp3", "audio/mpeg"},
    {".ogg", " audio/ogg"},
    {".png", "image/png"},
    {".svg", "image/svg+xml"},
    {".xml", "text/xml"},
    {NULL, NULL},
};


char *default_mime_type = "text/plain";


void get_time_str(char *timestr) {
    struct tm *tm_info;
    struct timeval tv;
    
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);
    
    strftime(timestr, 26, "%d/%m/%Y,%H:%M:%S:", tm_info);
    char msstr[100];
    int us = tv.tv_usec;
    sprintf(msstr,"%06d ", us);
    strcat(timestr, msstr);
}


void rio_init(rio_t *rp, int fd) {
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}


ssize_t writen(int fd, void *usrbuf, size_t n) {
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;

    while (nleft > 0) {
        if ((nwritten = write(fd, bufp, nleft)) <= 0) {
            if (errno == EINTR)  /* interrupted by sig handler return */
                nwritten = 0;    /* and call write() again */
            else
                return -1;       /* errorno set by write() */
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    return n;
}


int fd_can_read(int fd, int timeout) {
    struct pollfd pfd;

    pfd.fd = fd;
    pfd.events = POLLIN;

    if (poll(&pfd, 1, timeout)) {
        if (pfd.revents & POLLIN)
            return 1;
    }
    return 0;
}

/*
 * rio_read - This is a wrapper for the Unix read() function that
 *    transfers min(n, rio_cnt) bytes from an internal buffer to a user
 *    buffer, where n is the number of bytes requested by the user and
 *    rio_cnt is the number of unread bytes in the internal buffer. On
 *    entry, rio_read() refills the internal buffer via a call to
 *    read() if the internal buffer is empty.
 */
/* $begin rio_read */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n) {
    int cnt; char timestr[100];
    while (rp->rio_cnt <= 0) {  /* refill if buf is empty */
        if (fd_can_read(rp->rio_fd, TIMEOUT)) {
            rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));
        } else {
            get_time_str(timestr);
            printf("\033[1;94m\033[38;2;255;0;0m%s@%d : Robust IO: read timeout!\033[0m\n", timestr, getpid());
            return -1;
        }
        if (rp->rio_cnt < 0) {
            if (errno != EINTR) /* interrupted by sig handler return */
                return -1;
        }
        else if (rp->rio_cnt == 0)  /* EOF */
            return 0;
        else
            rp->rio_bufptr = rp->rio_buf; /* reset buffer ptr */
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


/*
 * rio_readlineb - robustly read a text line (buffered)
 */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) {
    int n, rc;
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++) {
        if ((rc = rio_read(rp, &c, 1)) == 1) {
            *bufp++ = c;
            if (c == '\n')
                break;
        } else if (rc == 0) {
            if (n == 1)
                return 0; /* EOF, no data read */
            else
                break;    /* EOF, some data was read */
        } else
            return -1;    /* error */
    }
    *bufp = 0;
    return n;
}


void format_size(char* buf, struct stat *stat) {
    if(S_ISDIR(stat->st_mode)) {
        sprintf(buf, "%s", "[DIR]");
    } else {
        off_t size = stat->st_size;
        if(size < 1024) {
            sprintf(buf, "%lu B", size);
        } else if (size < 1024 * 1024) {
            sprintf(buf, "%.2f KB", (double)size / 1024);
        } else if (size < 1024 * 1024 * 1024) {
            sprintf(buf, "%.2f MB", (double)size / 1024 / 1024);
        } else {
            sprintf(buf, "%.2f GB", (double)size / 1024 / 1024 / 1024);
        }
    }
}


void handle_directory_request(int out_fd, int dir_fd, char *filename) {
    char buf[MAXLINE], m_time[32], size[16];
    struct stat statbuf;
    sprintf(buf, "HTTP/1.1 200 OK\r\n%s%s%s%s%s",
            "Content-Type: text/html\r\n\r\n",
            "<html><head><style>",
            "body {font-family: monospace; font-size: 100px; text-align:center;}",
            "td {padding: 5px 6px;}",
            "</style></head><body><table align = center>\n");
    writen(out_fd, buf, strlen(buf));
    DIR *d = fdopendir(dir_fd);
    struct dirent *dp;
    int ffd;
    while ((dp = readdir(d)) != NULL) {
        if(!strcmp(dp->d_name, ".")) {
            continue;
        }
        if ((ffd = openat(dir_fd, dp->d_name, O_RDONLY)) == -1) {
            perror(dp->d_name);
            continue;
        }
        fstat(ffd, &statbuf);
        strftime(m_time, sizeof(m_time),
                 "%Y-%m-%d %H:%M", localtime(&statbuf.st_mtime));
        format_size(size, &statbuf);
        if(S_ISREG(statbuf.st_mode) || S_ISDIR(statbuf.st_mode)) {
            char *d = S_ISDIR(statbuf.st_mode) ? "/" : "";
            sprintf(buf, "<tr><td><a href=\"%s%s\">%s%s</a></td><td>%s</td><td>%s</td></tr>\n",
                    dp->d_name, d, dp->d_name, d, m_time, size);
            writen(out_fd, buf, strlen(buf));
        }
        close(ffd);
    }
    sprintf(buf, "</table></body></html>");
    writen(out_fd, buf, strlen(buf));
    closedir(d);
}


static const char* get_mime_type(char *filename) {
    char *dot = strrchr(filename, '.');
    if(dot) { // strrchar Locate last occurrence of character in string
        mime_map *map = mime_types;
        while(map->extension) {
            if(strcmp(map->extension, dot) == 0) {
                return map->mime_type;
            }
            map++;
        }
    }
    return default_mime_type;
}


int open_listenfd(int port) {
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    // 6 is TCP's protocol number
    // enable this, much faster : 4000 req/s -> 17000 req/s
    if (setsockopt(listenfd, 6, TCP_CORK,
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* Listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);

    // Bind syscall
    if (bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
}


void url_decode(char* src, char* dest, int max) {
    char *p = src;
    char code[3] = { 0 };
    while(*p && --max) {
        if(*p == '%') {
            memcpy(code, ++p, 2);
            *dest++ = (char)strtoul(code, NULL, 16);
            p += 2;
        } else {
            *dest++ = *p++;
        }
    }
    *dest = '\0';
}


int parse_request(int fd, http_request *req) {
    rio_t rio;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], timestr[100];
    req->offset = 0;
    req->end = 0;              /* default */
    rio_init(&rio, fd);
    if(rio_readlineb(&rio, buf, MAXLINE) < 0) {
        return -1;
    }
    get_time_str(timestr);
    printf("\033[1;94m\033[38;2;0;255;0m%s@%d : %s\033[0m", timestr, getpid(), buf);
    sscanf(buf, "%s %s", method, uri); /* version is not cared */
    /* read all */
    while(buf[0] != '\n' && buf[1] != '\n') { /* \n || \r\n */
        rio_readlineb(&rio, buf, MAXLINE);
        if(buf[0] == 'R' && buf[1] == 'a' && buf[2] == 'n') {
            sscanf(buf, "Range: bytes=%lu-%lu", &req->offset, &req->end);
            // Range: [start, end]
            if( req->end != 0) req->end ++;
        }
    }
    char* filename = uri;
    if(uri[0] == '/') {
        filename = uri + 1;
        int length = strlen(filename);
        if (length == 0) {
            filename = ".";
        } else {
            for (int i = 0; i < length; ++ i) {
                if (filename[i] == '?') {
                    filename[i] = '\0';
                    break;
                }
            }
        }
    }
    url_decode(filename, req->filename, MAXLINE);
    return 0;
}


void log_access(int status, struct sockaddr_in *c_addr, http_request *req) {
    char timestr[100];
    get_time_str(timestr);
    printf("%s@%d : %s:%d %d - %s\n", timestr ,getpid() ,inet_ntoa(c_addr->sin_addr),
           ntohs(c_addr->sin_port), status, req->filename);
}


void client_error(int fd, int status, char *msg, char *longmsg) {
    char buf[MAXLINE];
    sprintf(buf, "HTTP/1.1 %d %s\r\n", status, msg);
    sprintf(buf + strlen(buf),
            "Content-length: %lu\r\n\r\n", strlen(longmsg));
    sprintf(buf + strlen(buf), "%s", longmsg);
    writen(fd, buf, strlen(buf));
}


void serve_static(int out_fd, int in_fd, http_request *req,
                  size_t total_size) {
    char buf[256], temp[256], timestr[100]; int i, pid;
    if (req->offset > 0) {
        sprintf(buf, "HTTP/1.1 206 Partial\r\n");
        sprintf(buf + strlen(buf), "Content-Range: bytes %lu-%lu/%lu\r\n",
                req->offset, req->end, total_size);
    } else {
        sprintf(buf, "HTTP/1.1 200 OK\r\nAccept-Ranges: bytes\r\n");
    }
    sprintf(buf + strlen(buf), "Cache-Control: no-cache\r\n");
    // sprintf(buf + strlen(buf), "Cache-Control: public, max-age=315360000\r\nExpires: Thu, 31 Dec 2037 23:55:55 GMT\r\n");

    sprintf(buf + strlen(buf), "Content-length: %lu\r\n",
            req->end - req->offset);
    sprintf(buf + strlen(buf), "Content-type: %s\r\n\r\n",
            get_mime_type(req->filename));
    writen(out_fd, buf, strlen(buf));
    off_t offset = req->offset; /* copy */
    while(offset < req->end) {       
        for(i = 0; i < strlen(buf); i++)
            if(buf[i] == '\n' || buf[i] == '\r')
                temp[i] = ' ';
            else
                temp[i] = buf[i];
        pid = getpid();
        get_time_str(timestr);
        printf("\033[1;94m\033[38;2;255;100;255m%s@%d : %s\033[0m\n", timestr, pid, temp);
        if(sendfile(out_fd, in_fd, &offset, req->end - req->offset) <= 0) {
            break;
        }
        get_time_str(timestr);
        printf("%s@%d : offset:%li \n", timestr, pid, offset);
        close(out_fd);
        break;
    }
}


void process(int fd, struct sockaddr_in *clientaddr) {
    char timestr[100]; int pid = getpid();
    get_time_str(timestr);
    printf("%s@%d : accept request, fd is %d\n", timestr, pid, fd);
    http_request req;
    if(parse_request(fd, &req) < 0) {
        printf("%s@%d : this client didn't said anything further...\n", timestr, pid);
        return;
    }
    char filename[512];
    strcpy(filename, "./htdocs/"); // root directory of server
    strcat(filename, req.filename);
    struct stat sbuf;
    int status = 200, ffd = open(filename, O_RDONLY, 0);
    if(ffd <= 0) {
        status = 404;
        char *msg = "File not found";
        client_error(fd, status, "Not found", msg);
    } else {
        fstat(ffd, &sbuf);
        if(S_ISREG(sbuf.st_mode)) {
            if (req.end == 0) {
                req.end = sbuf.st_size;
            }
            if (req.offset > 0) {
                status = 206;
            }
            serve_static(fd, ffd, &req, sbuf.st_size);
        } else if(S_ISDIR(sbuf.st_mode)) {
            status = 200;
            handle_directory_request(fd, ffd, filename);
        } else {
            status = 400;
            char *msg = "Unknow Error";
            client_error(fd, status, "Error", msg);
        }
        close(ffd);
    }
    log_access(status, clientaddr, &req);
}


int get_int() {
    char *end, buf[1000];
    int n = 0;
    do {
        if (!fgets(buf, sizeof buf, stdin))
            break;
        buf[strlen(buf) - 1] = 0;
        n = strtol(buf, &end, 10);
    } while (end != buf + strlen(buf));
    return n;
}


void terminate(int sig) {
    char timestr[100];
    get_time_str(timestr);
    printf("%s@%d : terminating listener\n", timestr, getpid());
    exit(0);
}


void init_signal_handlers() {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = terminate;
    sigaction(SIGINT, &action, NULL);
    // ignore broken pipes 
    signal(SIGPIPE, SIG_IGN);
}


int main(int argc, char** argv) {
    struct sockaddr_in clientaddr;
    int port_no = 8000, listenfd, connfd, i, max_listeners = 20;
    char timestr[100];
    socklen_t clientlen = sizeof clientaddr;
    
    init_signal_handlers();
    
    if(argc == 2) {
        if(argv[1][0] >= '0' && argv[1][0] <= '9') {
            port_no = atoi(argv[1]);
        } else {
            if(chdir(argv[1]) != 0) {
                perror(argv[1]);
                exit(1);
            }
        }
    } else if (argc == 3) {
        port_no = atoi(argv[2]);
        max_listeners = atoi(argv[1]);
    }

    listenfd = open_listenfd(port_no);
    if (listenfd > 0) {
        printf("\033[1;94m\033[38;2;0;0;255m  < Micro HTTP server by @rvcgeeks >  \033[0m\n"
               "Server is:\n"
               "Listening on port %d\n"
               "With socket descriptor %d\n"
               "With %d listeners\n"
               "PRESS Ctrl - C to terminate.\n\n"
               "\033[48;2;255;0;0m\033[1;94m\033[38;2;255;255;255mDD/MM/YYYY,HH:MM:SS:u-secs @pid  : current action        \033[0m\n"
               , port_no, listenfd, max_listeners);
    } else {
        perror("ERROR");
        exit(126);
    }
    // Serve 
    for(;;) {
        for(i = 0; i < max_listeners; i++) {
            int pid = fork();
            if (pid == 0) {
                // always look for an incoming request
                for(;;) {
                    connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
                    process(connfd, &clientaddr);
                    close(connfd);
                }
            } else if (pid > 0) {
                get_time_str(timestr);
                printf("%s@%d : listner forked\n", timestr, pid);
            } else {
                perror("fork");
            }
        }
        max_listeners = get_int(); // if user enters a number fork those many listeners at runtime for scalability
    }
    
    // unreachable code 
}