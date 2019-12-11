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
#include <openssl/ssl.h>
#include <openssl/err.h>


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
SSL* ssl;

void writeLog(double temp) {
    char buff[256];
    time_t raw;
    struct tm* currTime;

    time(&raw);
    currTime = localtime(&raw);

    sprintf(buff, "%.2d:%.2d:%.2d %.1f\n", currTime->tm_hour, currTime->tm_min, currTime->tm_sec, temp);

    if(SSL_write(ssl, buff, strlen(buff))< 0){
        fprintf(stderr, "Error: unable to initialize temperature sensor.\n");
        mraa_deinit();
        exit(1);
    }

    if(lg && sdown == 0) {
        dprintf(lfd, "%.2d:%.2d:%.2d %.1f\n", currTime->tm_hour, currTime->tm_min, currTime->tm_sec, temp);
    }
}

double readSensor(int tempReading){
    double  temp = 1023.0 / (double)tempReading - 1.0;
    temp *= R0;
    float temperature = 1.0/(log(temp/R0)/B+1/298.15) - 273.15;
    return flag == 'C'? temperature: temperature*9/5 + 32; //convert to Fahrenheit
}


void parseCommands(const char* input) {
    if(strcmp(input, "OFF") == 0){
        if(lg){
            dprintf(lfd, "OFF\n");
        }
        
        time_t raw;
        struct tm* currTime;

        time(&raw);
        currTime = localtime(&raw);
        char buff[256];

        sprintf(buff, "%.2d:%.2d:%.2d SHUTDOWN\n", currTime->tm_hour, currTime->tm_min, currTime->tm_sec);

        if(SSL_write(ssl, buff, strlen(buff))< 0){
            fprintf(stderr, "Error: unable to initialize temperature sensor.\n");
            mraa_deinit();
            exit(1);
        }

        if(lg) {
            dprintf(lfd, "%.2d:%.2d:%.2d SHUTDOWN\n", currTime->tm_hour, currTime->tm_min, currTime->tm_sec);
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
        if(lg) {
            dprintf(lfd, "%s\n", input);
        }
    }
    else {
        fprintf(stderr, "Error: Command not recognized\n");
        exit(1);
    }
}


void setupPollandTime(){
    char cmdBuf[128];
    char cpyBuf[128];

    memset(cmdBuf, 0, 128);
    memset(cpyBuf, 0, 128);

    int copyIndex = 0;
    polls[0].fd = socketFd;
    polls[0].events = POLLIN | POLLERR | POLLHUP;

    for(;;){
        int value = mraa_aio_read(tempSensor);
        double tempValue = readSensor(value);
        if(!sdown){
            writeLog(tempValue);
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
                int num = SSL_read(ssl, cmdBuf, 128);
                if(num < 0){
                    fprintf(stderr, "Error: unable to read from ssl.\n");
                    exit(2);
                }

                int i;
                for(i = 0; i < num && copyIndex < 128; i++){
                    if(cmdBuf[i] =='\n'){
                        parseCommands((char*)&cpyBuf);
                        copyIndex = 0;
                        memset(cpyBuf, 0, 128); 
                    }
                    else {
                        cpyBuf[copyIndex] = cmdBuf[i];
                        copyIndex++;
                    }
                }
                
            }
            time(&end);
        }
    }
}

void initSSL() {
    OpenSSL_add_all_algorithms();
    if(SSL_library_init() < 0){
        fprintf(stderr, "Error: Unable to initialize SSL\n");
        exit(1);
    }

    SSL_CTX *ssl_ctx = SSL_CTX_new(TLSv1_client_method());
    if(ssl_ctx == NULL){
        fprintf(stderr, "Error: Unable to intialize SSL context\n");
        exit(2);
    }

    ssl = SSL_new(ssl_ctx);
    if(SSL_set_fd(ssl, socketFd)<0) {
        fprintf(stderr, "Error: Unable to associate file descriptor\n");
        exit(2);
    }

    if(SSL_connect(ssl) != 1){
        fprintf(stderr, "Error: Unable to establish ssl connection\n");
        exit(2);
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

                char* logFile = optarg;
                lfd = open(logFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);

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
        fprintf(stderr, "Invalid port number\n");
        exit(1);
    }

    close(STDIN_FILENO);

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

    char buffer[64];
    initSSL();
    sprintf(buffer, "ID=%s\n", idNum);
    if(SSL_write(ssl, buffer, strlen(buffer))< 0){
        fprintf(stderr, "Error: unable to initialize temperature sensor.\n");
        mraa_deinit();
        exit(1);
    }
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