#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
/*
 * Return codes:
 *     2 - wrong argument
 * */

#define PROGRAM_VERSION "1.5"

#define METHOD_GET     0
#define METHOD_HEAD    1
#define METHOD_OPTIONS 2
#define METHOD_TRACE   3

int http10 = 1;
/*
 * http/0.9 - 0
 * http/1.0 - 1
 * http/1.1 - 2
*/

int clients = 1;      // Concurrent number, default 1
int force = 0;        // Whether the data returned by the server needs to wait for reading.
int force_reload = 0; // Whether to use caching.

int proxyport = 80;   // Proxy server port

int benchTime = 30;

int method = METHOD_GET;

static void usage()
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

/*
struct option {
    const char *name;   // The name of the long parameter

    int  has_arg;
        // no_argument          0 (Indicates that the parameter is not followed by a parameter value.)
        // required_argument    1 (Indicates that the parameter must be followed by a parameter value.)
        // optional_argument    2 (Indicates that the parameter can be followed by or without a parameter value.)

    int *flag;  // Used to determine the return value of the function getopt_long ().
    int val;    // If flag is null, the return value is the val value that matches the item.
};
*/

static const struct option long_options[] =
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
    /*  If the user does not enter any arguments.  */
    if(argc == 1) {
        usage();
        exit(2);
    }

    /*  Parsing command line argument options.  */
    const char *optstring = "912frp:c:t:Vh?";
    /*
     * Common usage of [optstring]:
     *      1. Single character: indicates an option
     *      2. Single character followed by a colon:
     *          indicates that the option must be followed by a parameter.
     *          Arguments follow the options or are separated by spaces.
     *      3. A single character followed by two colons:
     *          this option can have parameters or no parameters.
     *          If there are parameters, they must be separated by spaces after the options.
     *      Note:
     *          optstring is a string representing acceptable parameters.
     *          For example, "a: b: cd" means that the acceptable parameters are a, b, c, d,
     *          where a and b parameters are followed by more parameter values.
     * */
    for(int opt = 0; (opt = getopt_long(argc, argv, optstring, long_options, NULL)) != EOF;) {
        switch(opt) {
            case 0: break;
            case 'f': force = 1; break;
            case 'r': force_reload = 1; break;

            case '9': http10 = 0; break;
            case '1': http10 = 1; break;
            case '2': http10 = 2; break;

            case 't': benchTime = atoi(optarg); break;
            case 'c': clients = atoi(optarg); break;

            case 'p': // proxy server parsing server: port.
            {
                char *tmp = strrchr(optarg, ':');
                if(tmp == NULL) {
                    break;
                }
                if(tmp == optarg) {
                    fprintf(stderr, "Error in option --proxy %s: Missing hostname.\n", optarg);
                    exit(2);
                }
                if(tmp == strlen(optarg)+optarg-1) {
                    fprintf(stderr, "Error in option --proxy %s Port number is missing.\n", optarg);
                    exit(2);
                }
                *tmp = '\0';
                proxyport = atoi(tmp+1); break;
            }

            case 'h':
            case '?': usage(); exit(2);

            case 'V': printf(PROGRAM_VERSION"\n"); exit(0);
            default: break;
        }
    }

    return 0;
}
