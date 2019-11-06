/* ___________________________________________________________
 * ____________<<<___#_R_V_C_G_E_E_K_S___>>>__________________
 * CREATED BY #RVCGEEKS @PUNE for more github.com/rvcgeeks
 *
 * Real Time Programming Development
 * micro auto scaling HTTP server in 500 lines
 * created on 26.10.2019 4:30am
 * 
 * main.c : entry point
*/

#include "decls.h"
#include "rio.c"
#include "http.c"
#include "server.c"


int main(int argc, char** argv) {
    
    int port_no = 8000; char timestr[100];
    
    STEM_PID = getpid();
    init_signal_handlers();
    if(argc == 2)
        port_no = atoi(argv[1]);

    if(deploy(port_no) == 0) {
        get_time_str(timestr);
        printf("\033[1;94m\033[38;2;0;0;255m  < Micro HTTP server by @rvcgeeks >  \033[0m\n"
               "Server is:\n"
               "Listening on port %d\n"
               "With socket descriptor %d\n"
               "PRESS Ctrl - C to terminate.\n\n"
               "\033[48;2;255;0;0m\033[1;94m\033[38;2;255;255;255mDD/MM/YYYY,HH:MM:SS:u-secs @pid  : current action        \033[0m\n"
               "%s@%d : Server successfully launched\a\n"
               , port_no, SRV_SOCK_FD, timestr, STEM_PID);
    } else {
        perror("ERROR");
        return -1;
    }
    
    loop();   // Stem never dies until SIGINT
    
    // unreachable code 
}
