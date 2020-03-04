/*
 * WebBench:
 * Simple website stress testing tool
 *     Create a child process to simulate the client, repeatedly send a get request
 *     to the target website within a certain time, and calculate the returned data.
 *
 * Usage:
 *     webbench --help
 *
 * Return codes:
 *     0 - success
 *     1 - benchmark failed (server is not on-line)
 *     2 - bad arguments
 *     3 - internal error, fork failed
 *
 * */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include "socket.c"


#define PROGRAM_VERSION "1.5"

#define METHOD_GET     0
#define METHOD_HEAD    1
#define METHOD_OPTIONS 2
#define METHOD_TRACE   3

int method = METHOD_GET;    // Default request method

#define HOSTINAMELEN 2560
#define REQUEST_SIZE 2048

char host[HOSTINAMELEN];    // Server IP address
char request[REQUEST_SIZE]; // HTTP request sent to the server

int http10 = 1;             // http/0.9 - 0 | http/1.0 - 1 | http/1.1 - 2

char *proxy_host = NULL;    // Proxy server IP

int proxy_port = 80;        // Proxy server port

int clients = 1;            // Concurrent number

int benchTime = 30;         // Client runtime

int force = 0;              // Whether to wait for a response from the server. (0 - Yes | 1 - No)

int force_reload = 0;       // Whether to use caching. (0 - Yes | 1 - No)

volatile int timer_expired = 0; // Whether the stress test duration is saturated.

int my_pipe[2];             // Parent and child processes communicate using pipes.

int speed  = 0;             // Number of processes responded by the server.
int bytes  = 0;             // Number of bytes read by the process.
int failed = 0;             // Number of processes not responding to the server.


// Function to generate a request HTTP message.
static void build_request(const char *URL);

// Timeout detection function: Use signals to control the end of the process.
static void alarm_handler(void) { timer_expired = 1; }

// Stress test function: Parent and child processes communicate through the pipeline,
//   child process stress test, parent process records
static int bench(void);

// Child process stress test function.
static void bench_core(const char *host, const int port, char *request);

// Tips for using functions: Note that the server-side URL should be given
//   at the end of the command line.
static void usage(void)
{
    fprintf(stderr,
        "webbench [option]... URL\n"
        "  -f|--force               Don't wait for reply from server.\n"
        "  -r|--reload              Send reload request - Pragma: no-cache.\n"
        "  -t|--time <sec>          Run benchmark for <sec> seconds. Default 30.\n"
        "  -p|--proxy <server:port> Use proxy server for request.\n"
        "  -c|--clients <n>         Run <n> HTTP clients at once. Default one.\n"
        "  -9|--http09              Use HTTP/0.9 style requests.\n"
        "  -1|--http10              Use HTTP/1.0 protocol.\n"
        "  -2|--http11              Use HTTP/1.1 protocol.\n"
        "  --get                    Use GET request method.\n"
        "  --head                   Use HEAD request method.\n"
        "  --options                Use OPTIONS request method.\n"
        "  --trace                  Use TRACE request method.\n"
        "  -?|-h|--help             This information.\n"
        "  -V|--version             Display program version.\n"
        );
}

// Detailed classification of command line arguments.
const struct option long_options[15] =
{
    {"help",    no_argument, NULL, '?'},
    {"version", no_argument, NULL, 'V'},

    {"http09",  no_argument, NULL, '9'},
    {"http10",  no_argument, NULL, '1'},
    {"http11",  no_argument, NULL, '2'},

    {"force",   no_argument, &force, 1},
    {"reload",  no_argument, &force_reload, 1},

    {"get",     no_argument, &method, METHOD_GET},
    {"head",    no_argument, &method, METHOD_HEAD},
    {"options", no_argument, &method, METHOD_OPTIONS},
    {"trace",   no_argument, &method, METHOD_TRACE},

    {"time",    required_argument, NULL, 't'},
    {"proxy",   required_argument, NULL, 'p'},
    {"clients", required_argument, NULL, 'c'},

    {NULL,0,NULL,0}
};


int main(int argc, char *argv[])
{
    //  If the user does not enter any arguments.
    if(argc == 1) {
        usage();
        exit(2);
    }

    //  Parsing command line argument options.
    const char *optstring = "912frp:c:t:Vh?";
    for(int opt = 0; (opt = getopt_long(argc, argv, optstring, long_options, NULL)) != EOF;) {
        switch(opt) {
            case  0 : break;

            case '9': http10 = 0; break;
            case '1': http10 = 1; break;
            case '2': http10 = 2; break;

            case 'f': force = 1; break;
            case 'r': force_reload = 1; break;
            case 't': benchTime = !atoi(optarg) ? 30 : atoi(optarg);
            case 'c': clients   = !atoi(optarg) ?  1 : atoi(optarg);
            case 'p': {
                char *tmp_proxyport = strrchr(optarg, ':');
                if(tmp_proxyport == NULL) { break; }
                if(tmp_proxyport == optarg) {
                    fprintf(stderr, "Error in option --proxy %s: Missing hostname.\n", optarg);
                    exit(2);
                }
                if(strlen(tmp_proxyport) == 1) {
                    fprintf(stderr, "Error in option --proxy %s Port number is missing.\n", optarg);
                    exit(2);
                }
                proxy_port = atoi(tmp_proxyport + 1);

                *tmp_proxyport = '\0'; // Separate IP address and port number.
                proxy_host = optarg;
                break;
            }
            case 'h':
            case '?': usage(); exit(2);
            case 'V': printf(PROGRAM_VERSION"\n"); exit(0);
            default: break;
        }
    }

    // Determine if a URL is missing.
    if(optind == argc) {
        fprintf(stderr, "WebBench: Missing URL!\n");
        usage();
        exit(2);
    }

    //  Copyright
    fprintf(stderr, "WebBench - Simple Web Benchmark "PROGRAM_VERSION"\n"
        "Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.\n");

    // Construct the HTTP request message body.
    build_request(argv[optind]);

    // Print information about this stress test.
    printf("\nBenchmarking: ");
    switch(method) {
        case METHOD_GET:
        default:             printf("GET");     break;
        case METHOD_HEAD:    printf("HEAD");    break;
        case METHOD_OPTIONS: printf("OPTIONS"); break;
        case METHOD_TRACE:   printf("TRACE");   break;
    }
    printf(" %s", argv[optind]);
    switch(http10) {
        case 0: printf(" (using HTTP/0.9)\n"); break;
        case 2: printf(" (using HTTP/1.1)\n"); break;
        default: break;
    }

    printf("%d client%s", clients, clients == 1 ? "" : "s");
    printf(", running %d seconds", benchTime);
    if(force) {
        printf(", early socket close");
    }
    if(proxy_host != NULL) {
        printf(", via proxy server %s:%d", proxy_host, proxy_port);
    }
    if(force_reload) {
        printf(", forcing reload");
    }
    printf(".\n");

    // Start stress test.
    return bench();
}


static void build_request(const char *URL)
{
    // Set the HTTP protocol version.
    if(force_reload && proxy_host != NULL && http10 < 1) { http10 = 1; }
    if(method == METHOD_HEAD    && http10 < 1) { http10 = 1; }
    if(method == METHOD_OPTIONS && http10 < 2) { http10 = 2; }
    if(method == METHOD_TRACE   && http10 < 2) { http10 = 2; }

    // Fill request method.
    bzero(request, REQUEST_SIZE);
    switch(method) {
        default:
        case METHOD_GET:     strcpy(request, "GET");     break;
        case METHOD_HEAD:    strcpy(request, "HEAD");    break;
        case METHOD_OPTIONS: strcpy(request, "OPTIONS"); break;
        case METHOD_TRACE:   strcpy(request, "TRACE");   break;
    }
    strcat(request, " ");

    // Check the legality of the URL(HTTP).
    if(strstr(URL, "://") == NULL) {
        fprintf(stderr, "\n%s: is not a valid URL.\n", URL);
        exit(2);
    }
    // Preventing buffer overflows.
    if(1500 < strlen(URL)) {
        fprintf(stderr, "URL is too long.\n");
        exit(2);
    }
    if(proxy_host == NULL && strncasecmp("http://", URL, 7) != 0) {
        fprintf(stderr, "\nOnly HTTP protocol is directly supported, set --proxy for others.\n");
        exit(2);
    }

    // Get the domain name address subscript based on the URL delimiter("://").
    int i = strstr(URL, "://") - URL + 3;

    // Delimiter at the end of the URL requires '/'.
    if(strchr(URL + i, '/') == NULL) {
        fprintf(stderr, "\nInvalid URL syntax - hostname don't ends with '/'.\n");
        exit(2);
    }

    if(proxy_host == NULL) { // No proxy server used.
        // Determine if the URL has a port number.
        bzero(host, HOSTINAMELEN);
        if(index(URL + i, ':') != NULL && index(URL + i, ':') < index(URL + i, '/')) {
            // Copy host address.
            strncpy(host, URL + i, strchr(URL + i, ':') - URL - i);
            // Copy host port.
            char tmp_port[10]; bzero(tmp_port, 10);
            strncpy(tmp_port, index(URL + i, ':') + 1, strchr(URL + i, '/') - index(URL + i, ':') - 1);
            proxy_port = !atoi(tmp_port) ? 80 : atoi(tmp_port);
        } else {
            strncpy(host, URL + i, strcspn(URL + i, '/'));
        }
        strcat(request+strlen(request), URL + i + strcspn(URL + i, '/'));
    } else { // Use a proxy server.
        strcat(request, URL);
    }

    if(http10 == 1) {
        strcat(request, "HTTP/1.0\r\n");
    } else if(http10 == 2) {
        strcat(request, "HTTP/1.1\r\n");
    }

    if(0 < http10) {
        strcat(request, "User-Agent: WebBench "PROGRAM_VERSION"\r\n");
        if(proxy_host == NULL) {
            strcat(request, "Host: ");
            strcat(request, host);
            strcat(request, "\r\n");
        }
    }

    if(force_reload && proxy_host != NULL) {
        strcat(request, "Pragma: no-cache\r\n");
    }
    if(1 < http10) {
        strcat(request, "Connection: close\r\n");
    }
    if(0 < http10) {
        strcat(request, "\r\n");
    }
}

static int bench(void)
{
    // The parent process makes a connection first to check the availability of the target server.
    int sock_fd = Socket(proxy_host == NULL ? host : proxy_host, proxy_port);
    if(sock_fd < 0) {
        fprintf(stderr, "\nConnect to server failed. Aborting benchmark.\n");
        return 1;
    }
    close(sock_fd);

    // create pipe
    if(pipe(my_pipe)) {
        perror("pipe failed.");
        return 3;
    }

    pid_t pid = 0;
    for(int i = 0; i < clients; ++i) {
        pid = fork();
        if(pid <= 0) {
            sleep(1);
            break;
        }
    }

    if(pid < 0) {
        fprintf(stderr, "problems forking worker no. %d\n", sock_fd);
        perror("fork failed.");
        return 3;
    }

    int i, j, k;

    if(pid == 0) {
        if(proxy_host == NULL) {
            bench_core(host  , proxy_port, request);
        } else {
            bench_core(proxy_host, proxy_port, request);
        }

        FILE *writef = fdopen(my_pipe[1], "w");
        if(writef == NULL) {
            perror("open pipe for writing failed.");
            return 3;
        }

        fprintf(writef, "%d %d %d\n", speed, failed, bytes);
        fclose(writef);
        return 0;
    } else {
        FILE *readf = fdopen(my_pipe[0], "r");
        if(readf == NULL) {
            perror("open pipe for reading failed.");
            return 3;
        }

        setvbuf(readf, NULL, _IONBF, 0);
        speed  = 0;
        failed = 0;
        bytes  = 0;

        while(1) {
            pid = fscanf(readf, "%d %d %d", &i, &j, &k);
            if(pid < 2) {
                fprintf(stderr, "Some of our childrens died.\n");
                break;
            }
            speed  += i;
            failed += j;
            bytes  += k;

            if(--clients == 0) {
                break;
            }
        }
        fclose(readf);

        printf("\nSpeed=%d pages/min, %d bytes/sec.\nRequests: %d susceed, %d failed.\n",
               (int)((speed+failed)/(benchTime/60.0f)),
               (int)(bytes/(float)benchTime),
               speed,
               failed);
    }

    return i;
}

static void bench_core(const char *host, const int port, char *request)
{
    // setup alarm signal handler.
    struct sigaction signal;
    signal.sa_handler = alarm_handler;
    signal.sa_flags   = 0;

    if(sigaction(SIGALRM, &signal, NULL)) {
        exit(3);
    }

    alarm(benchTime);

    int read_len = strlen(request);
    next_try:
    while(1) {
        if(timer_expired) {
            if(0 < failed) {
                failed--;
            }
            return;
        }

        int conn_fd = Socket(host, port);
        if(conn_fd < 0) {
            failed++;
            continue;
        }

        if(read_len != write(conn_fd, request, read_len)) {
            failed++;
            close(conn_fd);
            continue;
        }

        if(http10 == 0 && shutdown(conn_fd, 1)) {
            failed++;
            close(conn_fd);
            continue;
        }

        if(force == 0) {
            while(1) {
                if(timer_expired) {
                    break;
                }

                char buf[1500];
                int ret = read(conn_fd, buf, 1500);
                if(ret < 0) {
                    failed++;
                    close(conn_fd);
                    goto next_try;
                } else {
                    if(ret == 0) {
                        break;
                    } else {
                        bytes += ret;
                    }
                }
            }
        }

        if(close(conn_fd)) {
            failed++;
            continue;
        }

        speed++;
    }
}
