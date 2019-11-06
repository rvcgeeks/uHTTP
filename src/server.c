/* ___________________________________________________________
 * ____________<<<___#_R_V_C_G_E_E_K_S___>>>__________________
 * CREATED BY #RVCGEEKS @PUNE for more github.com/rvcgeeks
 *
 * Real Time Programming Development
 * micro auto scaling HTTP server in 500 lines
 * created on 26.10.2019 4:30am
 * 
 * server.c : server methods : children management, deployment, signal handling and server loop
*/


int deploy(int port) {
    int optval=1;
    struct sockaddr_in serveraddr;

    // Create a socket descriptor 
    if ((SRV_SOCK_FD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    // Eliminates "Address already in use" error from bind. 
    if (setsockopt(SRV_SOCK_FD, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    // 6 is TCP's protocol number
    // enable this, much faster : 4000 req/s -> 17000 req/s
    if (setsockopt(SRV_SOCK_FD, 6, TCP_CORK,
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;
    
    struct timeval timeout;      
    timeout.tv_sec = ACC_TIMEOUT / 1e3;
    timeout.tv_usec = 0;

    // set timeout on accept
    if (setsockopt(SRV_SOCK_FD, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                sizeof(timeout)) < 0)
        return -1;

    /* Listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);

    // Bind syscall
    if (bind(SRV_SOCK_FD, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    // Make it a listening socket ready to accept connection requests 
    if (listen(SRV_SOCK_FD, ACC_BACKLOG) < 0)
        return -1;
    return 0;
}


void terminate(int sig) {
    char timestr[100];
    get_time_str(timestr);
    if(STEM_PID == getpid())
        printf("%s@%d : terminating stem listener, fc = %d\n",
               timestr, getpid(), FORK_CNT);
    else {
        printf("\033[1;94m\033[38;2;255;127;0m%s@%d : listener terminated, fc = %d\033[0m\n",
            timestr, getpid(), FORK_CNT);
        kill(STEM_PID, SIGUNDRFLOW);
    }
    close(SRV_SOCK_FD);
    exit(0);
}


void loop() {
    int overflow_cnt = 0, connfd;
    struct timespec ts, te; 
    struct sockaddr_in clientaddr;
    socklen_t clientlen = sizeof clientaddr;
    char timestr[100]; double dt;
    
    for(;;) {  // Serve
        
        clock_gettime(CLOCK_MONOTONIC, &ts); 
        connfd = accept(SRV_SOCK_FD, (struct sockaddr *)&clientaddr, &clientlen);
        clock_gettime(CLOCK_MONOTONIC, &te); 
        
        dt = (te.tv_sec - ts.tv_sec) * 1e3 + (te.tv_nsec - ts.tv_nsec) * 1e-6; 
        
        if(connfd < 0) {
            if(STEM_PID != getpid())     // jobless and erred listeners terminated
                terminate(0);
            else 
                continue;
        }
        
        get_time_str(timestr);
        if(dt < ACC_LATENCY) {
            overflow_cnt++;
            printf("\033[1;94m\033[38;2;255;0;0m%s@%d : accept @%s:%d @fd = %d oc = %d dt = %f ms bottleneck at this listener\033[0m\n",
                   timestr ,getpid(), inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), connfd, overflow_cnt, dt);
        } else 
            printf("%s@%d : accept @%s:%d @fd = %d oc = %d dt = %f ms\n", timestr ,getpid(), 
                   inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), connfd, overflow_cnt, dt);
            
        process(connfd, &clientaddr);
        close(connfd);
        
        if(overflow_cnt >= ACC_OVERFLW) {  // self scaling server forks on more requests
            kill(STEM_PID, SIGOVERFLOW);      // signal the parent process to fork another process on overload
            overflow_cnt = 0;
        }
    }
}


void child_birth(int sig) {
    char timestr[100]; int pid;
    if(FORK_CNT > SRV_MAXCHLD) {  // avoid too muck forking
        get_time_str(timestr);
        printf("\033[1;94m\033[38;2;255;127;0m%s@%d : SERVER FULL ! too many listeners!\033[0m\n", timestr, getpid());
        return;
    }
    pid = fork();
    
    if(pid == 0)
        loop();
    else if(pid > 0) {
        FORK_CNT++;
        get_time_str(timestr);
        printf("\033[1;94m\033[38;2;0;0;255m%s@%d : listner forked from @%d, fc = %d \a\033[0m\n",
               timestr, pid, getpid(), FORK_CNT);
    } else 
        perror("ERROR");
}


void child_death(int sig) {
    FORK_CNT--;
}


void init_signal_handlers() {
    signal(SIGOVERFLOW , child_birth);  // our own signal to indicate the stem (parent) to fork a child
    signal(SIGUNDRFLOW , child_death);  // our own signal to indicate the stem (parent) to decrease fork count
    signal(     SIGINT , terminate);    // graceful exit on Ctrl - C
    signal(    SIGPIPE , SIG_IGN);      // ignore broken pipes 
    signal(    SIGCHLD , SIG_IGN);      // avoid zombie process
}