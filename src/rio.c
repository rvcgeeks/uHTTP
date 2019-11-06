/* ___________________________________________________________
 * ____________<<<___#_R_V_C_G_E_E_K_S___>>>__________________
 * CREATED BY #RVCGEEKS @PUNE for more github.com/rvcgeeks
 *
 * Real Time Programming Development
 * micro auto scaling HTTP server in 500 lines
 * created on 26.10.2019 4:30am
 * 
 * rio.c : Robust IO methods
 * http://www.cs.cmu.edu/afs/cs/academic/class/15213-f02/www/R11/section_a/Recitation11-SectionA.4up.pdf
*/

typedef struct {                 // Persistent state for the robust I/O (rio) 
    char  rio_buf[BUFFER_SIZE];  // internal buffer 
    char *rio_bufptr;            // next unread byte in this buf 
    int   rio_fd;                // descriptor for this buf 
    int   rio_cnt;               // unread byte in this buf 
} rio_t;


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


void rio_readninitb(rio_t *rp, int fd) {
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    memset(rp->rio_buf, 0, BUFFER_SIZE);
    rp->rio_bufptr = rp->rio_buf;
}


ssize_t rio_writen(int fd, void *usrbuf, size_t n) {
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;

    while (nleft > 0) {
        if ((nwritten = write(fd, bufp, nleft)) <= 0) {
            if (errno == EINTR)  // interrupted by sig handler return 
                nwritten = 0;    // and call write() again 
            else
                return -1;       // errorno set by write() 
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
 * rio_readn - This is a wrapper for the Unix read() function that
 *    transfers min(n, rio_cnt) bytes from an internal buffer to a user
 *    buffer, where n is the number of bytes requested by the user and
 *    rio_cnt is the number of unread bytes in the internal buffer. On
 *    entry, rio_readn() refills the internal buffer via a call to
 *    read() if the internal buffer is empty.
 */
static ssize_t rio_readn(rio_t *rp, char *usrbuf, size_t n) {
    int cnt; char timestr[100];
    
    while (rp->rio_cnt <= 0) {  // refill if buf is empty 
        if (fd_can_read(rp->rio_fd, RIO_TIMEOUT))
            rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));
        else {
            get_time_str(timestr);
            printf("\033[1;94m\033[38;2;255;127;0m%s@%d : Robust IO: read timeout!\033[0m\n", timestr, getpid());
            return -1;
        }
        if (rp->rio_cnt < 0) {
            if (errno != EINTR) // interrupted by sig handler return 
                return -1;
        }
        else if (rp->rio_cnt == 0)  // EOF 
            return 0;
        else
            rp->rio_bufptr = rp->rio_buf; // reset buffer ptr 
    }
    // Copy min(n, rp->rio_cnt) bytes from internal buf to user buf 
    cnt = n;
    if (rp->rio_cnt < n)
        cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}


/*
 * rio_readnlineb - robustly read a text line (buffered)
 */
ssize_t rio_readnlineb(rio_t *rp, void *usrbuf, size_t maxlen) {
    int n, rc;
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++) {
        if ((rc = rio_readn(rp, &c, 1)) == 1) {
            *bufp++ = c;
            if (c == '\n')
                break;
        } else if (rc == 0) {
            if (n == 1)
                return 0; // EOF, no data read 
            else
                break;    // EOF, some data was read 
        } else
            return -1;    // error 
    }
    *bufp = 0;
    return n;
}
