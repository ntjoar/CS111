/*
 * Name: Nathan Tjoar
 * UID: 005081232
 * Email: ntjoar@g.ucla.edu
 * */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>

void signal_handler(int signum){
    if(signum == SIGSEGV) {
        fprintf(stderr, "Segfault\n");
    }
    exit(4);
}

int main (int argc, char **argv){
    char* ifn = NULL;
    char* ofn = NULL;
    int ifd = STDIN_FILENO;
    int ofd = STDOUT_FILENO;
    static int segFault = 0;

    int c;
    while(1) {
        static struct option long_options[] =
        {
            {"input", required_argument, 0, 'i'},
            {"output", required_argument, 0, 'o'},
            {"segfault", no_argument, &segFault, 1},
            {"catch", no_argument, 0, 'c'},
            {"dump-core", no_argument, 0, 'd'},
            {0, 0, 0, 0}
        };

        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "i:o:scd", long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch(c) {
            case 'i': 
                if(ifn != NULL)
                    free(ifn);
                ifn = malloc(strlen(optarg) + 1);
                if(ifn == NULL) {
                    fprintf(stderr, "Cannot allocate memory for input filename\n");
                    if(ifn != NULL)
                        free(ifn);
                    if(ifn != NULL)
                        free(ifn);
                    exit(2); 
                }

                strcpy(ifn, optarg);
                ifd = open(ifn, O_RDONLY);

                if(ifd == -1) {
                    fprintf(stderr, "Fail to open input file %s. %s", ifn, strerror(errno));
                    if(ifn != NULL)
                        free(ifn);
                    if(ofn != NULL)
                        free(ofn);
                    exit(2);
                }

                close(0);
                dup2(ifd, 0);
                break;
            case 'o':
                if(ofn != NULL)
                    free(ofn);
                ofn = malloc(strlen(optarg) + 1);
                if(ofn == NULL) {
                    fprintf(stderr, "Cannot allocate memory for output filename\n");
                    if(ofn != NULL)
                        free(ofn);
                    if(ifn != NULL)
                        free(ofn);
                    exit(2);
                }

                strcpy(ofn, optarg);
                ofd = open(ofn, O_RDONLY | O_CREAT | O_WRONLY | O_TRUNC, 0666);

                if(ofd == -1) {
                    fprintf(stderr, "--output: Fail to open/create %s. %s", ofn, strerror(errno));
                    if(ifn != NULL)
                        free(ifn);
                    if(ofn != NULL)
                        free(ofn);
                    exit(3);
                }

                close(0);
                dup2(ofd, 0);
                break;
            case 'c':
                signal(SIGSEGV, signal_handler);
                break;
            case 'd':
                signal(SIGSEGV, SIG_DFL);
                break;
            case 0:
                break;
            default:
                fprintf(stderr, "Unrecognized argument. Usage: --input --output --catch --segfault --dump-core only\n");
                exit(1);
                break;
        }
    }

    if(segFault == 1) {
        if(ifn != NULL)
            free(ifn);
        if(ofn != NULL)
            free(ofn);
        
        char* nul = NULL;
        *nul = 42069;
    }
    
    char rw;
    int readSt;
    
    while((readSt = read(ifd, &rw, 1)) > 0) {
        if(write(ofd, &rw, sizeof(rw)) == -1) {
            fprintf(stderr, "Error writing. %s",strerror(errno));
	    exit(3);
        }
    }
        
    if(readSt == -1)
        return 1;
        
    if(ifn != NULL) {
        if(close(ifd) == -1) {
            fprintf(stderr, "Error closing file: %s. %s", ifn, strerror(errno));
            free(ifn);
            if(ofn != NULL)
                free(ofn);
            exit(2);
        }
        free(ifn);
    }
        
    if(ofn != NULL) {
        if(close(ofd) == -1) {
            fprintf(stderr, "Error closing file: %s. %s", ofn, strerror(errno));
            free(ofn);
            if(ifn != NULL)
                free(ifn);
            exit(3);
        }
        free(ofn);
    }
    return 0;
}
