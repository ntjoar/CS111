#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mraa.h>
#include <mraa/aio.h>
#include <poll.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include "fcntl.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

const int B = 4275;
const int R0 = 100000; 
int period = 1;
char flag = 'F';
struct pollfd polls[1];
int lfd;
int lg = 0;
int sdown = 0;
int port;
int socketFd = 0;
struct sockaddr_in addr;
struct hostent *server;
char* hostname = NULL;
char* idNum;
mraa_aio_context tempSensor;

void writeLog(double temp) {
    time_t readVal;
    struct tm* tm;
    
    time(&readVal);
    tm = localtime(&readVal);
    
    dprintf(socketFd, "%.2d:%.2d:%.2d %.1f\n", tm->tm_hour, tm->tm_min, tm->tm_sec, temp);
    
    if(lg && sdown == 0) {
        dprintf(lfd, "%.2d:%.2d:%.2d %.1f\n", tm->tm_hour, tm->tm_min, tm->tm_sec, temp);
    }
}

double readSensor(int tempReading){
    double  temp = 1023.0 / (double)tempReading - 1.0;
    temp *= R0;
    float temperature = 1.0/(log(temp/R0)/B+1/298.15) - 273.15;
    if(flag == 'C')
        return temperature;
    else 
        return temperature * 9/5 + 32;
}

void parseCommands(const char* input) {
    if(strcmp(input, "OFF") == 0){
        if(lg){
            dprintf(lfd, "OFF\n");
        }
        time_t readVal;
        struct tm* tm;

        time(&readVal);
        tm = localtime(&readVal);

        dprintf(socketFd, "%.2d:%.2d:%.2d SHUTDOWN\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
        if(lg) {
            dprintf(lfd, "%.2d:%.2d:%.2d SHUTDOWN\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
        }
        exit(0);
    }
    else if(strcmp(input, "START") == 0){
        sdown = 0;
        if(lg){
            dprintf(lfd, "START\n");
        }
    }
    else if(strcmp(input, "STOP") == 0){
        sdown = 1;
        if(lg){
            dprintf(lfd, "STOP\n");
        }
    }
    else if(strcmp(input, "SCALE=F") == 0){
        flag = 'F';
        if(lg && sdown == 0){
            dprintf(lfd, "SCALE=F\n");
        }
    }
    else if(strcmp(input, "SCALE=C") == 0){
        flag = 'C';
        if(lg && sdown == 0){
            dprintf(lfd, "SCALE=C\n");
        }
    }
    else if(strncmp(input, "PERIOD=", sizeof(char)*7) == 0){
        int newPeriod = (int)atoi(input+7);
        period = newPeriod;
        if(lg && sdown == 0){
            dprintf(lfd, "PERIOD=%d\n", period);
        }
    }
    else if((strncmp(input, "LOG", sizeof(char)*3) == 0)){
        if(lg){
            dprintf(lfd, "%s\n", input);
        }
    }
    else {
        fprintf(stderr, "Command not recognized\n");
        exit(1);
    }
}

void setupTCP() {
    socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if(socketFd < 0){
        fprintf(stderr, "Error: cannot create socket.\n");
        exit(2);
    }
    
    server = gethostbyname(hostname);
    if (server == NULL){
        fprintf(stderr, "Error: Could not retrieve host.\n");
        exit(1);
    } 
    memset((char*)&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    
    bcopy((char*)server->h_addr, (char*)&addr.sin_addr.s_addr, server->h_length);
    addr.sin_port = htons(port);
    
    if(connect(socketFd, (struct sockaddr*)&addr, sizeof(addr))< 0){
        fprintf(stderr, "Error: Cannot establish a connection to the server.\n");
        exit(2);
    }
}

void setupPollandTime(){
    char cpyBuf[128];
    char cmdBuf[128];
    int cpyID = 0;

    memset(cmdBuf, 0, 128);
    memset(cpyBuf, 0, 128);

    polls[0].fd = socketFd;
    polls[0].events = POLLIN | POLLERR | POLLHUP;

    for(;;){
        int val = mraa_aio_read(tempSensor);
        double tmpVal = readSensor(val);
        if(!sdown){
            writeLog(tmpVal);
        }

        time_t begin, end;
        time(&begin);
        time(&end); 

        while(difftime(end, begin) < period){
            int ret = poll(polls, 1, 0);
            if(ret < 0){
                fprintf(stderr, "Error: Unable to poll.\n");
                exit(2);
            }

            if(polls[0].revents && POLLIN){
                int num = read(socketFd, cmdBuf, 128);
                if(num < 0){
                    fprintf(stderr, "Error: cannot read from socket.\n");
                    exit(2);
                }
                int i;
                for(i = 0; i < num && cpyID < 128; i++){
                    if(cmdBuf[i] =='\n'){
                        parseCommands((char*)&cpyBuf);
                        cpyID = 0;
                        memset(cpyBuf, 0, 128); //clear
                    }
                    else {
                        cpyBuf[cpyID] = cmdBuf[i];
                        cpyID++;
                    }
                }
                
            }
            time(&end);
        }
    }
}

int main(int argc, char** argv){
    int opt = 0;
    static struct option options [] = {
        {"period", 1, 0, 'p'},
        {"scale", 1, 0, 's'},
        {"log", 1, 0, 'l'},
        {"id", 1, 0, 'i'},
        {"host", 1, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    while((opt=getopt_long(argc, argv, "p:sl", options, NULL)) != -1){
        switch(opt){
            case 'p':
                period = (int)atoi(optarg);
                if(period < 0){
                    fprintf(stderr, "Error: Period must be greater than 0.\n");
                    exit(1);
                }
                break;
            case 's':
                switch(*optarg){
                    case 'C':
                    case 'c':
                        flag = 'C';
                        break;
                    case 'F':
                    case 'f':
                        flag = 'F';
                        break;
                    default:
                        fprintf(stderr, "Error: invalid argument.\nUsage: ./lab4a --period=n --scale=F/C --log=filename\n");
                        exit(1);
                        break;
                }
                break;
            case 'l':
                lg = 1;

                char* lFile = optarg;
                lfd = open(lFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);

                if(lfd < 0){
                    fprintf(stderr, "Error: unable to open file.\n");
                    exit(2);
                }
                break;
            case 'i':
                idNum = optarg;
                break;
            case 'h':
                hostname = optarg;
                break;
            default:
                fprintf(stderr, "Error: invalid argument.\nUsage: ./lab4a --period=n --scale=F/C --log=filename\n");
                exit(1);
                break;
        }
    }
    
    port = atoi(argv[optind]);
    
    if(port <= 0){
        fprintf(stderr, "Error: Invalid port number\n");
        exit(1);
    }
    
    close(STDIN_FILENO);
    setupTCP();
    dprintf(socketFd, "ID=%s\n", idNum);
    dprintf(lfd, "ID=%s\n", idNum);
    
    tempSensor = mraa_aio_init(1);
    if(tempSensor == NULL){
        fprintf(stderr, "Error: unable to initialize temperature sensor.\n");
        mraa_deinit();
        exit(1);
    }

    setupPollandTime();

    mraa_aio_close(tempSensor);
    close(lfd);
    
    exit(0);
}
