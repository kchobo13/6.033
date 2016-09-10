#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <err.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdb.h>
#include <netinet/in.h>

enum { use_fork = 1 };

static FILE *cur_f;               /* current connection */
static const char *cur_dir = "."; /* web home directory */

static int
fgets_trim(char *buf, int size, FILE *f)
{
    char *p = fgets(buf, size, f);
    if (!p)
        return -1;

    /* Trim newline characters at the end of the line */
    while (buf[strlen(buf) - 1] == '\r' ||
           buf[strlen(buf) - 1] == '\n')
    buf[strlen(buf) - 1] = '\0';

    return 0;
}

static void
url_decode(char *src, char *dst)
{
    for (;;) {
        if (src[0] == '%' && src[1] && src[2]) {
            char hexbuf[3];
            hexbuf[0] = src[1];
            hexbuf[1] = src[2];
            hexbuf[2] = '\0';

            *dst = strtol(&hexbuf[0], 0, 16);
            src += 3;
        } else if (src[0] == '+') {
            *dst = ' ';
            src++;
        } else {
            *dst = *src;
            src++;

            if (*dst == '\0')
                break;
        }

        dst++;
    }
}

static const char *
parse_req(char *reqpath)
{
    static char buf[8192];        /* static variables are not on the stack */

    if (fgets_trim(&buf[0], sizeof(buf), cur_f) < 0)
        return "Socket IO error";

    /* Parse request like "GET /foo.html HTTP/1.0" */
    char *sp1 = strchr(&buf[0], ' ');
    if (!sp1)
        return "Cannot parse HTTP request (1)";
    *sp1 = '\0';
    sp1++;

    char *sp2 = strchr(sp1, ' ');
    if (!sp2)
        return "Cannot parse HTTP request (2)";
    *sp2 = '\0';
    sp2++;

    /* We only support GET requests */
    if (strcmp(&buf[0], "GET"))
        return "Non-GET request";

    /* Decode URL escape sequences in the requested path into reqpath */
    url_decode(sp1, reqpath);

    /* Parse out query string, e.g. "foo.py?user=bob" */
    char *qp = strchr(reqpath, '?');
    if (qp) {
        *qp = '\0';
        setenv("QUERY_STRING", qp+1, 1);
    }

    /* Now parse HTTP headers */
    for (;;) {
        if (fgets_trim(&buf[0], sizeof(buf), cur_f) < 0)
            return "Socket IO error";

        if (buf[0] == '\0')        /* end of headers */
            break;

        /* Parse things like "Cookie: foo bar" */
        char *sp = strchr(&buf[0], ' ');
        if (!sp)
            return "Header parse error (1)";
        *sp = '\0';
        sp++;

        /* Strip off the colon, making sure it's there */
        if (strlen(buf) == 0)
            return "Header parse error (2)";

        char *colon = &buf[strlen(buf) - 1];
        if (*colon != ':')
            return "Header parse error (3)";
        *colon = '\0';

        /* Set the header name to uppercase */
        for (int i = 0; i < strlen(buf); i++)
            buf[i] = toupper(buf[i]);

        /* Decode URL escape sequences in the value */
        char value[256];
        url_decode(sp, &value[0]);

        /* Store header in env. variable for application code */
        char envvar[256];
        sprintf(&envvar[0], "HTTP_%s", buf);
        setenv(envvar, value, 1);
    }

    return NULL;
}

static void
http_err(int code, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    fprintf(cur_f, "HTTP/1.0 %d Error\r\n", code);
    fprintf(cur_f, "Content-Type: text/html\r\n");
    fprintf(cur_f, "\r\n");
    fprintf(cur_f, "<H1>An error occurred</H1>\r\n");
    vfprintf(cur_f, fmt, ap);

    va_end(ap);
    fclose(cur_f);
    cur_f = NULL;
}

static void
process_client(int fd)
{
    register int ebp __asm__ ("ebp");
    char reqpath[256];
    char pn[1024];
    struct stat st;
    const char *errmsg;

    fprintf(stderr, "\nreqpath = %p\nebp     = 0x%x\nunlink  = %p\n",
            reqpath, ebp, unlink);

    cur_f = fdopen(fd, "w+");

    errmsg = parse_req(reqpath);
    if (errmsg) {
        http_err(500, "Error parsing request: %s", errmsg);
        return;
    }

    sprintf(pn, "%s/%s", cur_dir, reqpath);

    if (stat(pn, &st) < 0) {
        http_err(404, "File not found or not accessible: %s", pn);
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        /* For directories, use index.html in that directory */
        strcat(pn, "/index.html");
        if (stat(pn, &st) < 0) {
            http_err(404, "File not found or not accessible: %s", pn);
            return;
        }
    }

    if (S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR)) {
        /* executable bits -- run as CGI script */
        fflush(cur_f);

        signal(SIGCHLD, SIG_DFL);
        int pid = fork();
        if (pid < 0) {
            http_err(500, "Cannot fork: %s", strerror(errno));
            return;
        }

        if (pid == 0) {
            /* Child process */
            int nullfd = open("/dev/null", O_RDONLY);
            dup2(nullfd, 0);
            dup2(fileno(cur_f), 1);
            dup2(fileno(cur_f), 2);

            close(nullfd);
            fclose(cur_f);

            execl(pn, pn, 0);
            err(1, "execl");
        }

        int status;
        waitpid(pid, &status, 0);
        fclose(cur_f);
    } else {
        /* Non-executable: serve contents */
        int fd = open(pn, O_RDONLY);
        if (fd < 0) {
            http_err(500, "Cannot open %s: %s", pn, strerror(errno));
            return;
        }

        fprintf(cur_f, "HTTP/1.0 200 OK\r\n");
        fprintf(cur_f, "Content-Type: text/html\r\n");
        fprintf(cur_f, "\r\n");

        for (;;) {
            char readbuf[1024];
            int cc = read(fd, &readbuf[0], sizeof(readbuf));
            if (cc <= 0)
                break;

            fwrite(&readbuf[0], 1, cc, cur_f);
        }

        close(fd);
        fclose(cur_f);
    }
}

static void
print_server(int fd)
{
    char host[256];
    int port;
    struct hostent *hp;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    if (gethostname(host, sizeof(host)))
        err(1, "gethostname");
    hp = gethostbyname(host);
    if (!hp)
        errx(1, "gethostbyname: %s", hstrerror(h_errno));
    strcpy(host, hp->h_name);
    for (char *p = host; *p; ++p)
        *p = tolower(*p);

    if (getsockname(fd, &addr, &addrlen))
        err(1, "getsockname");
    port = ntohs(addr.sin_port);

    fprintf(stderr, "Web server running at %s:%d\n",
            host, port);
}

int
main(int argc, char **argv)
{
    int port = 4000;
    if (argc >= 2)
        port = atoi(argv[1]);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

    int srvfd = socket(AF_INET, SOCK_STREAM, 0);
    if (srvfd < 0)
        err(1, "socket");

    int on = 1;
    if (setsockopt(srvfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))
        err(1, "setsockopt SO_REUSEADDR");

    if (bind(srvfd, &sin, sizeof(sin))) {
        /* bind 4000 failed; try a random port */
        sin.sin_port = 0;
        if (bind(srvfd, &sin, sizeof(sin)))
            err(1, "bind");
    }

    if (listen(srvfd, 5))
        err(1, "listen");
    print_server(srvfd);

    signal(SIGCHLD, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    for (;;) {
        struct sockaddr_in client_addr;
        unsigned int addrlen = sizeof(client_addr);

        int cfd = accept(srvfd, (struct sockaddr *) &client_addr, &addrlen);
        if (cfd < 0) {
            perror("accept");
            continue;
        }

        int pid = use_fork ? fork() : 0;
        if (pid < 0) {
            perror("fork");
            close(cfd);
            continue;
        }

        if (pid == 0) {
            /* Child process. */
            if (use_fork)
                close(srvfd);

            process_client(cfd);

            if (use_fork)
                exit(0);
        }

        if (pid > 0) {
            /* Parent process. */
            close(cfd);
        }
    }
}
