/* ___________________________________________________________
 * ____________<<<___#_R_V_C_G_E_E_K_S___>>>__________________
 * CREATED BY #RVCGEEKS @PUNE for more github.com/rvcgeeks
 *
 * Real Time Programming Development
 * micro auto scaling HTTP server in 500 lines
 * created on 26.10.2019 4:30am
 * 
 * decls.h : necessary declarations : includes, macros, globals and constants for server
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <poll.h>                // set timeout to read a GET request 
#include <errno.h>
#include <arpa/inet.h>           // inet_ntoa 
#include <netinet/tcp.h>
#include <sys/sendfile.h>

#define   SIGOVERFLOW   SIGRTMIN + 1 // real time signal for making stem fork children
#define   SIGUNDRFLOW   SIGRTMIN + 2 // real time signal for indicating stem to decrease fork count

#define   BUFFER_SIZE   1024
const int RIO_TIMEOUT = 3000;    // 3s timeout on idle read  
const int ACC_TIMEOUT = 60000;   // 1min timeout for accept 
const int ACC_LATENCY = 10;      // 10ms minimum interval between 2 requests in one listener 
const int ACC_OVERFLW = 3;       // fork if more than 3 accept has delays less than ACC_LATENCY 
const int ACC_BACKLOG = 128;     // there can be 128 waiting requests in accept 
const int SRV_MAXCHLD = 300;     // max 300 forks

int SRV_SOCK_FD;                 // kept global for signal handling 
int STEM_PID;
int FORK_CNT = 0;
