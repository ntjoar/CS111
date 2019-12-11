#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>
#include <poll.h>
#include <math.h>
#include <mraa/gpio.h>
#include <mraa/aio.h>
#include <signal.h>
#include <errno.h>

static int shutdown = 0;
static char scale = 'F';
static int period = 1;
static int logFD = -1;
static int lg = 0;

sig_atomic_t volatile run = 1;

/* What to do when interrupt occurs */
void do_when_button_pressed() {
    shutdown = 1;
}

/* Parse commands in run */
int parseCommands(char c[]) {
    char buf[2000] = {0};
    unsigned int l;

    if (strcmp(c, "SCALE=F\n") == 0) {
        scale = 'F';
    } else if (strcmp(c, "SCALE=C\n") == 0) {   
        scale = 'C';
    } else if(c[0] == 'P') {
        unsigned int i = 0;
        if((l = strlen(c)) < 9) {
            fprintf(stderr, "Error: invalid argument %s. Usage: --period=[value]\n", c);
            exit(1);
        }
        for(i = 0; i < l; i++) {
            if(c[i] == '\n') {
                break;
            }
        }

        memcpy(buf, c, 7);
        if (strcmp(buf, "PERIOD=") != 0) {
            fprintf(stderr, "Error: invalid argument %s. Usage: PERIOD=[value].\n", c);
            exit(1);
        }

        memcpy(buf, &c[7], i-7);
        buf[i - 7] = 0;

        period = atoi(buf);
        if(period < 0)  {
            fprintf(stderr, "Error: period must be greater than 0.\n");
            exit(1);
        }
    } else if(strcmp(c, "STOP\n") == 0) {
        if(lg) 
            fprintf(stdout, "STOPPING\n");
        lg = 1; // Give exception here
    } else if (strcmp(c, "START\n") == 0) {
        if(!lg) 
            fprintf(stdout, "STARTING\n");
        lg = 0;
    } else if(strcmp(c, "OFF\n") == 0) {
        shutdown = 1;
    } else if (c[0] == 'L'){
        l = strlen(c);
        if(l < 3) {
            fprintf(stderr, "Error: invalid argument %s. Usage: LOG [some message].\n", c);
            exit(1);
        }
        memcpy(buf, &c[4], l-4); 
        buf[l-4]=0;
        fprintf(stdout, "Received Message: %s\n", buf);
        fprintf(stdout, "%s", buf);
    } else {
        fprintf(stderr, "Error: invalid argument: %s.\n", c);
        exit(1);
    }
    return 0;
}

/* Print to Log */
void writeLog(int fd, float tmp) {
    time_t timeVal;
    struct tm *in;

    time(&timeVal);
    in = localtime(&timeVal);

    int s = in->tm_sec;
    int m = in->tm_min;
    int h = in->tm_hour;

    if(h < 10)
        dprintf(fd, "0");
    dprintf(fd, "%i:", h);
    if(m < 10) 
        dprintf(fd, "0");
    dprintf(fd, "%i:", m);
    if(s < 10) 
        dprintf(fd, "0");
    dprintf(fd, "%i ", s);

    if(shutdown) 
        dprintf(fd, "SHUTDOWN\n");
    else
        dprintf(fd, "%.1f\n", tmp);
}

int main(int argc, char* const argv[]) {
    /* Parse said arguments */
    static struct option long_options[] = {
        {"period", required_argument, 0, 'p'},
        {"scale", required_argument, 0, 's'},
        {"log", required_argument, 0, 'l'},
        {0, 0, 0, 0}
    };

    int c;
    while((c = getopt_long(argc, argv, "", long_options, 0)) != -1) {
        switch(c) {
            case 'p':
                period = atoi(optarg);
                if(period < 0)  {
                    fprintf(stderr, "Error: period must be greater than 0.\n");
                    exit(1);
                }
                break;
            case 's':
                scale = *optarg;
                if(scale != 'F' && scale != 'C') {
                    fprintf(stderr, "Error: scale can only be in F or C. Scale is case sensitive.\n");
                    exit(1);
                }
                break;
            case 'l':
                logFD = open(optarg, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if(logFD == -1) {
                    fprintf(stderr, "Error: Unable to open log file: %s.\n", optarg);
                    exit(1);
                }
                break;
            default:
                fprintf(stderr, "Error: invalid argument: %s. Usage: ./lab4b ----scale=[fc] --period=[value] --log=[fileName]\n", optarg);
                exit(1);
                break;
        }
    }

    int tempRead;
    int parseRet;

    mraa_aio_context tempSen;
    mraa_gpio_context btn;

    btn = mraa_gpio_init(60);
    tempSen = mraa_aio_init(1);

    mraa_gpio_dir(btn, MRAA_GPIO_IN);
    mraa_gpio_isr(btn, MRAA_GPIO_EDGE_RISING, &do_when_button_pressed, NULL);

    struct pollfd pList[1];
    pList[0].fd = STDIN_FILENO;
    pList[0].events = POLLIN | POLLHUP | POLLERR;
    pList[0].revents = 0;

    while(run) {
        if (poll(pList, 1, 0) == -1) {
            fprintf(stderr, "Error: Unable to poll. %s\n", strerror(errno));
            exit(1);
        }

        if (pList[0].revents & POLLIN) {
            char c[2000];
            fgets(c, 2000, stdin);
            parseRet = parseCommands(c);
            if (parseRet == 0) {
                if (logFD != -1)
                    dprintf(logFD, "%s", c);
            }
        }

        tempRead = mraa_aio_read(tempSen);
        float t = 1023.0/tempRead-1.0;
        t = 100000 * t;

        float temp = 1.0/(log(t/100000) / 4275 + 1/298.15) - 273.15;
        
        if (scale == 'F') 
            temp = 32+temp/5*9;
        if(lg == 0)
            writeLog(STDOUT_FILENO, temp);      
            if (logFD != -1)
                writeLog(logFD, temp);
        if (shutdown) {
            if (lg == 1) {
                writeLog(STDOUT_FILENO, temp);
                if (logFD != -1)
                    writeLog(logFD, temp);
            }
            break;
        }
        usleep(period*1000000);
    }

    mraa_gpio_close(btn);
    mraa_aio_close(tempSen);

    return 0;
}
