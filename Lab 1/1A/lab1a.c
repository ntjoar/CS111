/* Name: Nathan Tjoar
 * UID: 005081232
 * Email: ntjoar@g.ucla.edu
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>

static int verboseAct = 0;

int main(int argc, char** argv){
    int len = argc;
    int fd[argc]; // Array of file descriptors
    int fdInfo [len];
    int fdN = 0; // Keeps track of our fd at
    int ifd; // inFileDes
    int ofd; // outFileDes
    int i; // iterator for loops
    int exitStatus = 0; // For non-fatal errors, thus allowing program to run without interruption
    char * optArg[argc];
    char * commandStats[argc];
    char * strStat; 
    char tempBuff[1000]; // Buffer for our command options
    int in, out, err; // --command parameters
    pid_t pid[argc]; // Likely max case of threads needed
    int subC; // Subcommands

    int c;
    for (i = 0; i < len; i++) { // initialize fds as -1 otherwise they're all valid as 0 -> even though these should be unusable
		fd[i] = -1;
        commandStats[i] = 0;
	}

    for (i = 0; i < argc; i++) {
		optArg[i] = NULL; // No options should be available
	}

    subC = 0;
    while(1) {
        static struct option long_options[] =
        {
            {"rdonly", required_argument, 0, 'r'},
            {"wronly", required_argument, 0, 'w'},
            {"command", required_argument, 0, 'c'},
            {"verbose", no_argument, 0, 'v'},
            {0, 0, 0, 0}
        };

        /* getopt_long stores the option index here. */
        int option_index = 0;
        c = getopt_long (argc, argv, "rwcv", long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1) {
            for (i=0; i < len; i++) {
				close(fd[i]);
			}
            break;
        }

        if (optarg != NULL) // Command that follows option up with a command should give this weird error
			if (optarg[0] == '-')
				if (optarg[1] == '-') {		
					optind--;
					fprintf(stderr, "Syntax error: --%s requires exactly 0 arguments\n", long_options[option_index].name);
                    exitStatus = 1;
					continue;
				}

		i = 1;
        optArg[0] = optarg;
		while (optind < argc) { 
			if (argv[optind][0] == '-') { // stop when @ next option
				if (argv[optind][1] == '-')
					break;
			}
			optArg[i] = argv[optind];
            optind++;
			i++;
		}
		optArg[i] = NULL;

        i = 0;
		strcpy(tempBuff, "\0"); // Buffer holds all the option arguments
		while (optArg[i] != NULL) { // concatenate optArg into one string
			strcat(tempBuff, optArg[i]);
			strcat(tempBuff, " ");
			i++;
		}
        optArg[i] = NULL;

        if(long_options[option_index].has_arg == no_argument) { // no argument needed
            if(optArg[1] != NULL) {
                fprintf(stderr, "Syntax error: --%s requires exactly 0 arguments\n", long_options[option_index].name);
                exitStatus = 1;
				continue;
            }
        }
        else if(long_options[option_index].has_arg == required_argument) {
            if(c == 'c') { // command option should have 4 arguments min
                if(optArg[3] == NULL) {
                    fprintf(stderr, "Syntax error: --command requires a minimum of four arguments\n");
                    exitStatus = 1;
					continue;
                }
            }
            else {
                if (optArg[0] == NULL) { // No arguments given
                    fprintf(stderr, "Syntax error: --%s requires an argument\n", long_options[option_index].name);
                    exitStatus = 1;
					continue;
                }
                else if (optArg[1] != NULL) { // More than one argument
                    fprintf(stderr, "Syntax error: --%s requires an argument\n", long_options[option_index].name);
                    exitStatus = 1;
					continue;
                }
            }
        }

		//format the string to be printed out if --verbose was declared
		asprintf(&strStat, "--%s %s", long_options[option_index].name, tempBuff);
        if  (verboseAct == 1)
		    fprintf(stdout, "%s \n", strStat);

        switch(c) {
            case 'r': // O_RDONLY
                ifd = open(optarg, O_RDONLY);

                if(ifd == -1) {
                    fprintf(stderr, "Fail to open input file %s. %s\n", optarg, strerror(errno));
                    exitStatus = 1;
                }
                fd[fdN] = ifd;
                fdN++;
                break;
            case 'w': // O_WRONLY
                ofd = open(optarg, O_WRONLY);

                if(ofd == -1) {
                    fprintf(stderr, "--output: Fail to open %s. %s\n", optarg, strerror(errno));
                    exitStatus = 1;
                }
                fd[fdN] = ofd;
                fdN++;
                break;

                if(fdN > len) {
                    exit(1); // Something went very wrong
                } 
            case 'c':
                if(isdigit(optArg[0][0])) { // Error handling 
                    in = atoi(optArg[0]);
                    if(in > fdN) {
                        fprintf(stderr, "Syntax error: Command input file descriptor is incorrect: %s\n", optArg[0]);
                        exitStatus = 1;
                    }
                }
                else{
                    fprintf(stderr, "Syntax error: Command input file descriptor parameter must be a a digit: %s\n", optArg[0]);
					exitStatus = 1;
					break;
                }
                if(isdigit(optArg[1][0])) {
                    out = atoi(optArg[1]);
                    if(out >= fdN) {
                        fprintf(stderr, "Syntax error: Command output file descriptor is incorrect: %s\n", optArg[1]);
                        exitStatus = 1;
                    }
                }
                else{
                    fprintf(stderr, "Syntax error: Command output file descriptor parameter must be a a digit: %s\n", optArg[1]);
					exitStatus = 1;
					break;
                }
                if(isdigit(optArg[2][0])) {
                    err = atoi(optArg[2]);
                    if(err >= fdN) {
                        fprintf(stderr, "Syntax error: Command error file descriptor is incorrect: %s\n", optArg[2]);
                        exitStatus = 1;
                    }
                }
                else{
                    fprintf(stderr, "Syntax error: Command error file descriptor parameter must be a a digit: %s\n", optArg[2]);
					exitStatus = 1;
					break;
                }

                pid[subC] = fork();// Keep track of all the child process id numbers to be waited on at the end
				if (pid[subC] == -1) // Failed process fork
					fprintf(stderr,"System call fork failed: %s\n", strerror(errno));
                else if(pid[subC] == 0) { // Child
                    if(dup2(fd[in], 0) == -1) { // In dup2
                        fprintf(stderr, "System call dup2 failed on logical file descriptor %d: %s\n", in, strerror(errno));
                        exit(1);
                    }
                    if(close(fd[in]) == -1) { // In close
                        fprintf(stderr, "System call close failed on logical file descriptor %d: %s\n", in, strerror(errno));
                        exit(1);
                    }
                    if(dup2(fd[out], 1) == -1) { // Out dup2
                        fprintf(stderr, "System call dup2 failed on logical file descriptor %d: %s\n", out, strerror(errno));
                        exit(1);
                    }
                    if(close(fd[out]) == -1) { // Out close
                        fprintf(stderr, "System call close failed on logical file descriptor %d: %s\n", out, strerror(errno));
                        exit(1);
                    }
                    if(dup2(fd[err], 2) == -1) { // Err dup2
                        fprintf(stderr, "System call dup2 failed on logical file descriptor %d: %s\n", err, strerror(errno));
                        exit(1);
                    }
                    if(close(fd[err]) == -1) { // Err close
                        fprintf(stderr, "System call close failed on logical file descriptor %d: %s\n", err, strerror(errno));
                        exit(1);
                    }

                    if (fdInfo[in] > 0)
						if (fdInfo[in+1] > 0)
							close(fd[in+1]);
					if (fdInfo[out] > 0)
						if (fdInfo[out-1] > 0)
							close(fd[out-1]);

                    if (execvp(optArg[3], optArg+3) == -1) {
						fprintf(stderr,"System call execvp failed: %s\n", strerror(errno));
						exit(1); 
					}
                    exit(exitStatus);
                }
                else { // Parent thread
                    if (fdInfo[in] > 0)	{ // if input is the read end of the pipe, close it
						fdInfo[in] = -1;
                        if (close(fd[in]) == -1) {
                            exitStatus = 1;
                            fprintf(stderr, "System call close failed on logical file descriptor %d: %s\n", in, strerror(errno));
                        }
                        fdInfo[in] = -1;
					}
					if (fdInfo[out] > 0) { // if output is the write end of the pipe, close it 
						if (close(fd[out]) == -1) {
                            exitStatus = 1;
							fprintf(stderr, "System call close failed on logical file descriptor %d: %s\n", out, strerror(errno));
						}
                        fdInfo[out] = -1;
					}

                    // Concatenate string with format "--command optarg1 optarg2"
					asprintf(&commandStats[subC], "%s %s\n", long_options[option_index].name, tempBuff);
					subC++; 
                }
                break;
            case 'v':
                verboseAct = 1;
                break;
            default:
                fprintf(stderr, "Unrecognized argument. Usage: --rdonly --wronly --command --verbose only\n");
                exitStatus = 1;
                break;
        }
        free(strStat);
    } // End of get_opt :(
    exit(exitStatus);
}
