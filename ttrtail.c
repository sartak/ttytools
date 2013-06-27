/*
 * vim:et:
 * */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

/* settings */

char            *base_path = "/home/sartak/tmp/laptop.ttyrec" ;
char            *base_ext = "ttyrec" ;
char            *server_addr = "213.184.131.118" ;
unsigned short  server_port = 31337 ;
char            *my_name = "eidolaptop" ;
char            *my_pw = "blahblah" ;

/* global state */

char    *curr_path = NULL ;
size_t  curr_pos = 0 ;
int     curr_fd = 0 ;
int     server_sock = 0 ;
int watchers = 0 ;
int most_watchers = 0 ;

/* aux */

char    frame[10240] ;
int     frame_size ;
char    init1[3] ;
char    init2[3] ;

void    die(const char *fmt, ...) {
    va_list vl ;
    va_start(vl, fmt) ;
    vfprintf(stderr, fmt, vl) ;
    va_end(vl) ;
    exit(-1) ;
}

void    die2(const char *fmt, ...) {
    va_list vl ;
    va_start(vl, fmt) ;
    vfprintf(stderr, fmt, vl) ;
    fprintf(stderr, ": %s\n", strerror(errno)) ;
    va_end(vl) ;
    exit(-1) ;
}

void    session_close(void) {
    printf("closing session.\n") ;

    if (curr_path) {
        free(curr_path) ;
        curr_path = NULL ;
    }

    if (server_sock) {
        close(server_sock) ;
        server_sock = 0 ;
    }
    
    if (curr_fd) {
        close(curr_fd) ;
        curr_fd = 0 ;
        curr_pos = 0 ;
    }
}

long    nothtonl(long x) {
    unsigned char *p = (void *)&x ;
    return ((((p[3] << 8) | p[2]) << 8) | p[1] << 8) | p[0] ;
}

int     read_frame(void) {
    long        hdr[3] ;
    int         st = read(curr_fd, hdr, 12);
    int         pos = curr_pos ;
    static int  nfails = 0 ;

    if (st == -1) die2("frame header read()") ;

    if (st < 12) {
        if (st && lseek(curr_fd, curr_pos, SEEK_SET) == -1)
            die2("lseek1 (%d)\n", curr_pos) ;
        return 0 ;
    }
    pos += 12 ;

    hdr[2] = nothtonl(hdr[2]) ;

    if (hdr[2] < 0 || hdr[2] > sizeof(frame)) {
        if (nfails ++ > 100)
            die("huh? not trying to read %d bytes\n", hdr[2]) ;
        else {
            if (st && lseek(curr_fd, curr_pos, SEEK_SET) == -1)
                die2("lseek1 (%d)\n", curr_pos) ;
            return 0 ;
        }
    }

    nfails = 0 ;

    st = read(curr_fd, frame, hdr[2]) ;
    if (st == -1) die2("frame data read()") ;
    if (st != hdr[2]) {
        if (st && lseek(curr_fd, curr_pos, SEEK_SET) == -1) die2("lseek2") ;
        return 0 ;
    }
    pos += st ;
    curr_pos = pos ;
    return frame_size = st ;
}

char    *memstr(char *mem, size_t len, const char *str) {
    int lstr = strlen(str) ;
    char *p ;

    while ((p = memchr(mem, *str, len))) {
         if ((len - (mem - p)) < lstr) return NULL ;
         if (strncmp(p, str, lstr) == 0) return p ;
         len -= (mem - p) + 1 ;
         mem = p + 1 ;
    }
    return NULL ;
}

void    skip_some(void) {
    size_t      best_pos = curr_pos, pos = curr_pos ;
    char        *p ;

    memset(init1, 0, 3) ;
    memset(init2, 0, 3) ;

    printf("skipping some... ") ;

    while (read_frame()) {
        if ((p = memstr(frame, frame_size, "\033)"))) {
            memcpy(init1, p, 3) ;
        } else if ((p = memstr(frame, frame_size, "\033("))) {
            memcpy(init2, p, 3) ;
        }
        if (memstr(frame, frame_size, "\033[2J")) {
            best_pos = pos ;
        }
        pos = curr_pos ;
    }
    printf("(%d bytes)\n", best_pos) ;
    if (lseek(curr_fd, best_pos, SEEK_SET) == -1) die2("lseek3") ;
    curr_pos = best_pos ;
}

void    session_open(const char *fname) {
    struct sockaddr_in          sai ;
    char                        hello[128] ;
    size_t                      hlen ;
    
    curr_fd = open(fname, O_RDONLY) ;
    if (curr_fd == -1) die2("open(`%s')", fname) ;
    curr_path = strdup(fname) ;
    curr_pos = 0 ;
    watchers = 0 ;

    printf("using %s\n", fname);

    skip_some() ;

    printf("connecting to %s:%d...\n", server_addr, server_port) ;

    server_sock = socket(PF_INET, SOCK_STREAM, 0) ;
    if (server_sock == -1) die2("socket") ;

    sai.sin_family = AF_INET;
    sai.sin_port = htons(server_port);
    sai.sin_addr.s_addr = inet_addr(server_addr) ;

    if (connect(server_sock, (void *)&sai, sizeof(sai)) == -1) {
        printf("connect(%s:%d): %s\nthrottling...\n",
                server_addr, server_port, strerror(errno)) ;
        sleep(5) ;
        session_close() ;
        return ;
    }


    hlen = snprintf(hello, sizeof(hello) - 1,
            "hello %.16s %.16s\n", my_name, my_pw) ;
    if (write(server_sock, hello, hlen) != hlen) die2("send()") ;

    if (*init1 && write(server_sock, init1, 3) != 3) die2("send()") ;
    if (*init2 && write(server_sock, init2, 3) != 3) die2("send()") ;
}


int     scan_dir(void) {
    DIR                 *d ;
    time_t              best_time = 0 ;
    size_t              best_size = 0 ;
    struct dirent       *de ;
    struct stat         st ;
    char                fullpath[PATH_MAX], best[PATH_MAX] ;

    /* printf("scanning '%s' for a new ttyrec\n", base_path) ; */

    /*
    *best = 0 ;

    d = opendir(base_path) ;
    if (!d) die2("opendir(`%s')", base_path) ;

    while ((de = readdir(d))) {
        if (*de->d_name == '.') continue ;
        if (base_ext) {
            char *p = strrchr(de->d_name, '.') ;
            if (!p || !(* ++p)) continue ;
            if (strcmp(p, base_ext)) continue ;
        }
        snprintf(fullpath, PATH_MAX - 1, "%s/%s", base_path, de->d_name) ;
        if (stat(fullpath, &st) == -1) {
            fprintf(stderr, "??? stat(`%s'): %s\n", fullpath, strerror(errno)) ;
            continue ;
        }
        
        if (!S_ISREG(st.st_mode)) continue ;
        if (st.st_mtime <= best_time) continue ;
        strcpy(best, fullpath) ;
        best_time = st.st_mtime ;
        best_size = st.st_size ;
    }

    closedir(d) ;
    */

    strcpy(best, base_path);

    if (*best) {
        if (curr_fd) {
            if (strcmp(curr_path, best) == 0 && best_size >= curr_pos)
                return 0 ;
            session_close() ;
        }
        session_open(best) ;
    }


    return 1 ;
}

char    *sleep_read(int sex, int usex) {
    fd_set rfds ;
    fd_set efds ;
    struct timeval tv ;
    int rv ;
    static char s[1024] ;

    FD_ZERO(&rfds) ;
    FD_ZERO(&efds) ;
    FD_SET(server_sock, &rfds) ;
    FD_SET(server_sock, &efds) ;

    tv.tv_sec = sex ;
    tv.tv_usec = usex ;

    rv = select(server_sock + 1, &rfds, NULL, &efds, &tv) ;

    if (rv == -1) die2("select") ;
    if (rv) {
        int st ;
        if (FD_ISSET(server_sock, &efds)) {
            printf("socket error\n") ;
            session_close() ;
            return NULL ;
        }

        if (!FD_ISSET(server_sock, &rfds)) return NULL ;
        memset(s, 0, sizeof(s)) ;
        st = read(server_sock, s, sizeof(s) - 1) ;
        if (st > 0) return s ;
        printf("problems with server socket, restarting\n") ;
        session_close() ;
    }

    return NULL ;
}

int     main(int ac, char **av) {
    unsigned n = 0 ;

#if 0
    if (ac == 1) {
        die(
            "usage: %s <username> [<password>] [<server ip>] [<server port>]\n"
            "  <username> parameter is mandatory, others are optional,\n"
            "             defaults will be used if unspecified.\n"
            "  default password '%s' (INSECURE).\n"
            "  default server ip is '%s'. you must specify numeric ip, sorry.\n"
            "  default server port is %d.\n",
            *av, my_pw, server_addr, server_port) ;
    }
#endif

    if (ac >= 2) my_name = av[1] ;
    if (ac >= 3) my_pw = av[2] ;
    if (ac >= 4) server_addr = av[3] ;
    if (ac >= 5) server_port = atoi(av[4]) ;

    printf("username '%s', server %s:%d\n", my_name, server_addr, server_port) ;

    scan_dir();
    for (;;) {
        if (curr_fd && read_frame()) {
            n = 0 ;
            if (write(server_sock, frame, frame_size) != frame_size)
                session_close() ;
        } else if (server_sock) {
            char *s = sleep_read(0, 200000) ;
            if (s) {
                if (!strcmp(s, "msg watcher connected\n")) {
                    ++ watchers ;
                    printf("New watcher! Up to %d.", watchers) ;
                    if (watchers > most_watchers) {
                        most_watchers = watchers ;
                        printf(
                            " That's a new record since "
                            "this session has started!") ;
                    }
                    printf("\n") ;
                } else if (!strcmp(s, "msg watcher disconnected\n")) {
                    -- watchers ;
                    printf("Lost a watcher. Down to %d.\n", watchers) ;
                } else
                    printf(">> %s", s) ;
            }
        } else {
            sleep(5) ;
            n = 100 ;
        }
    }
    return 1 ;
}
