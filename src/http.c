/* ___________________________________________________________
 * ____________<<<___#_R_V_C_G_E_E_K_S___>>>__________________
 * CREATED BY #RVCGEEKS @PUNE for more github.com/rvcgeeks
 *
 * Real Time Programming Development
 * micro auto scaling HTTP server in 500 lines
 * created on 26.10.2019 4:30am
 * 
 * http.c : http parser and listener
*/


typedef struct {
    char   filename[BUFFER_SIZE];
    off_t  offset;               // for support Range 
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
    char buf[BUFFER_SIZE], m_time[32], size[16];
    struct stat statbuf;
    sprintf(buf, "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n\r\n"
            "<html><head><style>"
            "body {font-family: monospace; font-size: 100px; text-align:center;}"
            "td {padding: 5px 6px;}"
            "</style></head><body><table align = center>\n");
    rio_writen(out_fd, buf, strlen(buf));
    DIR *d = fdopendir(dir_fd);
    struct dirent *dp;
    int ffd;
    while ((dp = readdir(d)) != NULL) {
        if(!strcmp(dp->d_name, "."))
            continue;
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
            rio_writen(out_fd, buf, strlen(buf));
        }
        close(ffd);
    }
    sprintf(buf, "</table></body></html>");
    rio_writen(out_fd, buf, strlen(buf));
    closedir(d);
}


static const char* get_mime_type(char *filename) {
    char *dot = strrchr(filename, '.');
    if(dot) { // strrchar Locate last occurrence of character in string
        mime_map *map = mime_types;
        while(map->extension) {
            if(strcmp(map->extension, dot) == 0)
                return map->mime_type;
            map++;
        }
    }
    return default_mime_type;
}


void url_decode(char* src, char* dest, int max) {
    char *p = src;
    char code[3] = { 0 };
    while(*p && --max) {
        if(*p == '%') {
            memcpy(code, ++p, 2);
            *dest++ = (char)strtoul(code, NULL, 16);
            p += 2;
        } else
            *dest++ = *p++;
    }
    *dest = '\0';
}


int parse_request(int fd, http_request *req) {
    rio_t rio;
    char buf[BUFFER_SIZE], method[BUFFER_SIZE], uri[BUFFER_SIZE], timestr[100];
    req->offset = 0;
    req->end = 0;              // default 
    rio_readninitb(&rio, fd);
    if(rio_readnlineb(&rio, buf, BUFFER_SIZE) < 0)
        return -1;
    get_time_str(timestr);
    printf("\033[1;94m\033[38;2;0;255;0m%s@%d : %s\033[0m", timestr, getpid(), buf);
    sscanf(buf, "%s %s", method, uri); // version is not cared
    // read all 
    while(buf[0] != '\n' && buf[1] != '\n') { //       \n || \r\n 
        rio_readnlineb(&rio, buf, BUFFER_SIZE);
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
    url_decode(filename, req->filename, BUFFER_SIZE);
    return 0;
}


void client_error(int fd, int status, char *msg, char *longmsg) {
    char buf[BUFFER_SIZE];
    sprintf(buf, "HTTP/1.1 %d %s\r\n", status, msg);
    sprintf(buf + strlen(buf),
            "Content-length: %lu\r\n\r\n", strlen(longmsg));
    sprintf(buf + strlen(buf), "%s", longmsg);
    rio_writen(fd, buf, strlen(buf));
}


void serve_static(int out_fd, int in_fd, http_request *req,
                  size_t total_size) {
    char buf[256], temp[256], timestr[100]; int i, pid;
    memset(buf, 0, 256); memset(temp, 0, 256);
    if (req->offset > 0) {
        sprintf(buf, "HTTP/1.1 206 Partial\r\n");
        sprintf(buf + strlen(buf), "Content-Range: bytes %lu-%lu/%lu\r\n",
                req->offset, req->end, total_size);
    } else {
        sprintf(buf, "HTTP/1.1 200 OK\r\nAccept-Ranges: bytes\r\n");
    }
// sprintf(buf + strlen(buf), "Cache-Control: no-cache\r\n");
// sprintf(buf + strlen(buf), "Cache-Control: public, max-age=315360000\r\nExpires: Thu, 31 Dec 2037 23:55:55 GMT\r\n");

    sprintf(buf + strlen(buf), "Content-length: %lu\r\n",
            req->end - req->offset);
    sprintf(buf + strlen(buf), "Content-type: %s\r\n\r\n",
            get_mime_type(req->filename));
    rio_writen(out_fd, buf, strlen(buf));
    off_t offset = req->offset; // copy 
    while(offset < req->end) {       
        for(i = 0; i < strlen(buf); i++)
            if(buf[i] == '\n' || buf[i] == '\r') temp[i] = ' ';
            else temp[i] = buf[i];
        pid = getpid(); get_time_str(timestr);
        printf("\033[1;94m\033[38;2;255;100;255m%s@%d : %s\033[0m\n", timestr, pid, temp);
        if(sendfile(out_fd, in_fd, &offset, req->end - req->offset) <= 0)
            break;
        close(out_fd);
        break;
    }
}


void process(int fd, struct sockaddr_in *clientaddr) {
    http_request req;
    memset(req.filename, 0, BUFFER_SIZE);
    if(parse_request(fd, &req) < 0)
        return;
    char filename[512], timestr[100];
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
    get_time_str(timestr);
    printf("%s@%d : done\n", timestr ,getpid());
}
