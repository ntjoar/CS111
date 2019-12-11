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
#include <sys/time.h>
#include <sys/resource.h>

static int verboseAct = 0;
static int optFlag = 0;
static int waitVal = 0;
static int sigExit = 0;
static int proFlag = 0;

/* Absolutely useless... IDK why, but it makes my code look cool B) */
void flagReset() { 
    optFlag = 0;
}

void catch(int signum){
    fprintf(stderr, "%d caught\n", signum);
    exit(signum);
}

int main(int argc, char** argv){
    struct rusage usage;
    int len = argc;
    int fdPipe[2];
    int fd[argc * 2]; // Array of file descriptors
    int fdN = 0; // Keeps track of our fd at
    int iofd; // inFileDes
    int i , j; // iterator for loops
    int exitStatus = 0; // For non-fatal errors, thus allowing program to run without interruption
    char * optArg[argc];
    char * commandStats[argc];
    char * commandStats2[argc];
    char * strStat; 
    char tempBuff[1000]; // Buffer for our command options
    char tempBuff2[1000]; // Buffer for our command options
    int in, out, err; // --command parameters
    pid_t pid[argc * 2]; // Likely max case of threads needed
    int subC; // Subcommands
    int status;
    struct timeval uStartTime, uEndTime, kStartTime, kEndTime;
    double totalUser = 0, totalKernel = 0;

    int c;
    for (i = 0; i < argc * 2; i++) { // initialize fds as -1 otherwise they're all valid as 0 -> even though these should be unusable
		fd[i] = -1;
	}

    for (i = 0; i < argc; i++) {
		optArg[i] = NULL; // No options should be available
        commandStats[i] = 0;
	}

    subC = 0;
    while(1) {
        static struct option long_options[] =
        {
            {"rdonly", required_argument, 0, 'r'},
            {"rdwr", required_argument, 0, 'z'},
            {"wronly", required_argument, 0, 'w'},
            {"close", required_argument, 0, 'x'},
            {"command", required_argument, 0, 'c'},
            {"catch", required_argument, 0, 'b'},
            {"ignore", required_argument, 0, 'i'},
            {"default", required_argument, 0, 'd'},
            {"chdir", required_argument, 0, 'H'},
            {"pause", no_argument, 0, 'P'},
            {"pipe", no_argument, 0, 'p'},
            {"wait", no_argument, 0, 'W'},
            {"abort", no_argument, 0, 'a'},
            {"verbose", no_argument, 0, 'v'},
            {"append", no_argument, 0, 'A'},
            {"cloexec", no_argument, 0, 'Y'},
            {"creat", no_argument, 0, 'C'},
            {"directory", no_argument, 0, 'D'},
            {"dsync", no_argument, 0, 'S'},
            {"excl", no_argument, 0, 'X'},
            {"nofollow", no_argument, 0, 'F'},
            {"nonblock", no_argument, 0, 'B'},
            {"rsync", no_argument, 0, 'R'},
            {"sync", no_argument, 0, 'N'},
            {"trunc", no_argument, 0, 'T'},
            {"profile", no_argument, 0, 'q'},
            {0, 0, 0, 0}
        };

        /* getopt_long stores the option index here. */
        int option_index = 0;
        c = getopt_long (argc, argv, "rwcv", long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

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

        if(proFlag == 1) {
            if(getrusage(RUSAGE_SELF, &usage) == -1)
                fprintf(stderr, "Error getting usage. %s\n", strerror(errno));
            uStartTime = usage.ru_utime;
            kStartTime = usage.ru_stime;
        }

        i = 0;
		strcpy(tempBuff, "\0"); // Buffer holds all the option arguments
		while (optArg[i] != NULL) { // concatenate optArg into one string
			strcat(tempBuff, optArg[i]);
			strcat(tempBuff, " ");
			i++;
		}
        if(c == 'c'){
            j = 3;
            strcpy(tempBuff2, "\0");
            while (optArg[j] != NULL) { // concatenate optArg into one string
                strcat(tempBuff2, optArg[j]);
                strcat(tempBuff2, " ");
                j++;
		    }   
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
		// format the string to be printed out if --verbose was declared
		asprintf(&strStat, "--%s %s", long_options[option_index].name, tempBuff);
        if  (verboseAct == 1)
		    fprintf(stdout, "%s \n", strStat);

        switch(c) {
            case 'A': // O_APPEND
                optFlag = optFlag | O_APPEND;
                break;
            case 'Y': // O_CLOEXEC
                optFlag = optFlag | O_CLOEXEC;
                break;
            case 'C': // O_CREAT
                optFlag = optFlag | O_CREAT;
                break;
            case 'D': // O_DIRECTORY
                optFlag = optFlag | O_DIRECTORY;
                break;
            case 'S': // O_DSYNC
                optFlag = optFlag | O_DSYNC;
                break;
            case 'X': // O_EXCL
                optFlag = optFlag | O_EXCL;
                break;
            case 'F': // O_NOFOLLOW
                optFlag = optFlag | O_NOFOLLOW;
                break;
            case 'B': // O_NONBLOCK
                optFlag = optFlag | O_NONBLOCK;
                break;
            case 'R': // O_RSYNC
                optFlag = optFlag | O_RSYNC; // Not working on Mac
                break;
            case 'N': // O_SYNC
                optFlag = optFlag | O_SYNC;
                break;
            case 'T': // O_TRUNC
                optFlag = optFlag | O_TRUNC;
                break;
            case 'r': // O_RDONLY
                iofd = open(optarg, O_RDONLY | optFlag, 0666);
                flagReset();

                if(iofd == -1) {
                    fprintf(stderr, "Fail to open input file %s. %s\n", optarg, strerror(errno));
                    exitStatus = 1;
                }
                fd[fdN] = iofd;
                fdN++;
                if(proFlag == 1) {
                    if(getrusage(RUSAGE_SELF, &usage) == -1)
                            fprintf(stderr, "Error getting usage. %s\n", strerror(errno));
                    uEndTime = usage.ru_utime;
                    kEndTime = usage.ru_stime;
                    totalUser += ((double)uEndTime.tv_sec - (double)uStartTime.tv_sec) + (((double)uEndTime.tv_usec - (double)uStartTime.tv_usec)/1000000);
                    totalKernel += ((double)kEndTime.tv_sec - (double)kStartTime.tv_sec) + (((double)kEndTime.tv_usec - (double)kStartTime.tv_usec)/1000000);
                    fprintf(stdout, "%s: user: %f, kernel: %f, total: %f\n", strStat, 
                                totalUser, totalKernel, totalUser + totalKernel);
                }
                break;
            case 'z': // O_RDWR
                iofd = open(optarg, O_RDWR | optFlag, 0666);
                flagReset();

                if(iofd == -1) {
                    fprintf(stderr, "Fail to open file %s. %s\n", optarg, strerror(errno));
                    exitStatus = 1;
                }
                fd[fdN] = iofd;
                fdN++;
                if(proFlag == 1) {
                    if(getrusage(RUSAGE_SELF, &usage) == -1)
                            fprintf(stderr, "Error getting usage. %s\n", strerror(errno));
                    uEndTime = usage.ru_utime;
                    kEndTime = usage.ru_stime;
                    totalUser += ((double)uEndTime.tv_sec - (double)uStartTime.tv_sec) + (((double)uEndTime.tv_usec - (double)uStartTime.tv_usec)/1000000);
                    totalKernel += ((double)kEndTime.tv_sec - (double)kStartTime.tv_sec) + (((double)kEndTime.tv_usec - (double)kStartTime.tv_usec)/1000000);
                    fprintf(stdout, "%s: user: %f, kernel: %f, total: %f\n", strStat, 
                                totalUser, totalKernel, totalUser + totalKernel);
                }
                break;
            case 'w': // O_WRONLY
                iofd = open(optarg, O_WRONLY | optFlag, 0666);
                flagReset();

                if(iofd == -1) {
                    fprintf(stderr, "--output: Fail to open %s. %s\n", optarg, strerror(errno));
                    exitStatus = 1;
                }
                fd[fdN] = iofd;
                fdN++;
                if(fdN > len) {
                    exit(1); // Something went very wrong
                } 
                if(proFlag == 1) {
                    if(getrusage(RUSAGE_SELF, &usage) == -1)
                            fprintf(stderr, "Error getting usage. %s\n", strerror(errno));
                    uEndTime = usage.ru_utime;
                    kEndTime = usage.ru_stime;
                    totalUser += ((double)uEndTime.tv_sec - (double)uStartTime.tv_sec) + (((double)uEndTime.tv_usec - (double)uStartTime.tv_usec)/1000000);
                    totalKernel += ((double)kEndTime.tv_sec - (double)kStartTime.tv_sec) + (((double)kEndTime.tv_usec - (double)kStartTime.tv_usec)/1000000);
                    fprintf(stdout, "%s: user: %f, kernel: %f, total: %f\n", strStat, 
                                totalUser, totalKernel, totalUser + totalKernel);
                }
                break;
            case 'x': // close
                if (close(fd[atoi(optarg)]) < 0) {
                    exitStatus = 1;
                    fprintf(stderr, "--close: Fail to close %s. %s\n", optarg, strerror(errno));
                }
                else
                    fd[atoi(optarg)] = -1;
                if(proFlag == 1) {
                    if(getrusage(RUSAGE_SELF, &usage) == -1)
                            fprintf(stderr, "Error getting usage. %s\n", strerror(errno));
                    uEndTime = usage.ru_utime;
                    kEndTime = usage.ru_stime;
                    totalUser += ((double)uEndTime.tv_sec - (double)uStartTime.tv_sec) + (((double)uEndTime.tv_usec - (double)uStartTime.tv_usec)/1000000);
                    totalKernel += ((double)kEndTime.tv_sec - (double)kStartTime.tv_sec) + (((double)kEndTime.tv_usec - (double)kStartTime.tv_usec)/1000000);
                    fprintf(stdout, "%s: user: %f, kernel: %f, total: %f\n", strStat, 
                                totalUser, totalKernel, totalUser + totalKernel);
                }
                break;
            case 'i': // ignore
                signal(atoi(optarg), SIG_IGN);
                if(proFlag == 1) {
                    if(getrusage(RUSAGE_SELF, &usage) == -1)
                            fprintf(stderr, "Error getting usage. %s\n", strerror(errno));
                    uEndTime = usage.ru_utime;
                    kEndTime = usage.ru_stime;
                    totalUser += ((double)uEndTime.tv_sec - (double)uStartTime.tv_sec) + (((double)uEndTime.tv_usec - (double)uStartTime.tv_usec)/1000000);
                    totalKernel += ((double)kEndTime.tv_sec - (double)kStartTime.tv_sec) + (((double)kEndTime.tv_usec - (double)kStartTime.tv_usec)/1000000);
                    fprintf(stdout, "%s: user: %f, kernel: %f, total: %f\n", strStat, 
                                totalUser, totalKernel, totalUser + totalKernel);
                }
                break;
            case 'a': // abort
                free(strStat);
                fflush(stdout);
                raise(SIGSEGV);
                if(proFlag == 1) {
                    if(getrusage(RUSAGE_SELF, &usage) == -1)
                            fprintf(stderr, "Error getting usage. %s\n", strerror(errno));
                    uEndTime = usage.ru_utime;
                    kEndTime = usage.ru_stime;
                    totalUser += ((double)uEndTime.tv_sec - (double)uStartTime.tv_sec) + (((double)uEndTime.tv_usec - (double)uStartTime.tv_usec)/1000000);
                    totalKernel += ((double)kEndTime.tv_sec - (double)kStartTime.tv_sec) + (((double)kEndTime.tv_usec - (double)kStartTime.tv_usec)/1000000);
                    fprintf(stdout, "%s: user: %f, kernel: %f, total: %f\n", strStat, 
                                totalUser, totalKernel, totalUser + totalKernel);
                }
                break;
            case 'b': // catch
                signal(atoi(optarg), catch);
                if(proFlag == 1) {
                    if(getrusage(RUSAGE_SELF, &usage) == -1)
                            fprintf(stderr, "Error getting usage. %s\n", strerror(errno));
                    uEndTime = usage.ru_utime;
                    kEndTime = usage.ru_stime;
                    totalUser += ((double)uEndTime.tv_sec - (double)uStartTime.tv_sec) + (((double)uEndTime.tv_usec - (double)uStartTime.tv_usec)/1000000);
                    totalKernel += ((double)kEndTime.tv_sec - (double)kStartTime.tv_sec) + (((double)kEndTime.tv_usec - (double)kStartTime.tv_usec)/1000000);
                    fprintf(stdout, "%s: user: %f, kernel: %f, total: %f\n", strStat, 
                                totalUser, totalKernel, totalUser + totalKernel);
                }
                break;
            case 'p': // pipe
                if(pipe(fdPipe) < 0) {
                    exitStatus = 1;
                    fprintf(stderr, "Pipe error. Reason: %s\n", strerror(errno));
                }
                else {
                    fd[fdN] = fdPipe[0];
                    fd[fdN + 1] = fdPipe[1];
                    fdN += 2;
                }
                if(proFlag == 1) {
                    if(getrusage(RUSAGE_SELF, &usage) == -1)
                            fprintf(stderr, "Error getting usage. %s\n", strerror(errno));
                    uEndTime = usage.ru_utime;
                    kEndTime = usage.ru_stime;
                    totalUser += ((double)uEndTime.tv_sec - (double)uStartTime.tv_sec) + (((double)uEndTime.tv_usec - (double)uStartTime.tv_usec)/1000000);
                    totalKernel += ((double)kEndTime.tv_sec - (double)kStartTime.tv_sec) + (((double)kEndTime.tv_usec - (double)kStartTime.tv_usec)/1000000);
                    fprintf(stdout, "%s: user: %f, kernel: %f, total: %f\n", strStat, 
                                totalUser, totalKernel, totalUser + totalKernel);
                }
                break;
            case 'W': // wait
                for(i = waitVal; i < subC; i++) {
                    if(waitpid(pid[i], &status, 0) != -1) {
                        if (WIFEXITED(status)) { 
                            fprintf(stdout, "exit %d %s", WEXITSTATUS(status), commandStats2[i]);
                            if(exitStatus < WEXITSTATUS(status))
                                exitStatus = WEXITSTATUS(status);
                        }
                        else if(WIFSIGNALED(status)){
                            fprintf(stdout, "signal %d %s", WTERMSIG(status), commandStats2[i]);
                            if(sigExit < WTERMSIG(status))
                                sigExit = WTERMSIG(status);;
                        }
                    } // END Waitpid
                    else
                        abort(); // Something went wrong
                } // endfor

                waitVal = subC;
                if(proFlag == 1) {
                    if(getrusage(RUSAGE_SELF, &usage) == -1)
                            fprintf(stderr, "Error getting usage. %s\n", strerror(errno));
                    uEndTime = usage.ru_utime;
                    kEndTime = usage.ru_stime;
                    totalUser += ((double)uEndTime.tv_sec - (double)uStartTime.tv_sec) + (((double)uEndTime.tv_usec - (double)uStartTime.tv_usec)/1000000);
                    totalKernel += ((double)kEndTime.tv_sec - (double)kStartTime.tv_sec) + (((double)kEndTime.tv_usec - (double)kStartTime.tv_usec)/1000000);
                    fprintf(stdout, "%s: user: %f, kernel: %f, total: %f\n", strStat, 
                                totalUser, totalKernel, totalUser + totalKernel);
                    // Children
                    if(getrusage(RUSAGE_CHILDREN, &usage) == -1)
                        fprintf(stderr, "Error getting child process usage. %s\n", strerror(errno));
                    uEndTime = usage.ru_utime;
                    kEndTime = usage.ru_stime;
                    totalUser = ((double)uEndTime.tv_sec - (double)uStartTime.tv_sec) + (((double)uEndTime.tv_usec - (double)uStartTime.tv_usec)/1000000);
                    totalKernel = ((double)kEndTime.tv_sec - (double)kStartTime.tv_sec) + (((double)kEndTime.tv_usec - (double)kStartTime.tv_usec)/1000000);
                    fprintf(stdout, "Child: user: %f, kernel: %f, total: %f\n", 
                                totalUser, totalKernel, totalUser + totalKernel);
                }
                break;
            case 'c': // command i o e cmd 
                if(isdigit(optArg[0][0])) { // Error handling 
                    in = atoi(optArg[0]);
                    if(in >= fdN) {
                        fprintf(stderr, "Syntax error: Command input file descriptor is incorrect: %s\n", optArg[0]);
                        exitStatus = 1;
                        break;
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
                        fprintf(stderr, "Syntax error: Command input file descriptor is incorrect: %s\n", optArg[1]);
                        exitStatus = 1;
                        break;
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
                        break;
                    }
                }
                else{
                    fprintf(stderr, "Syntax error: Command error file descriptor parameter must be a a digit: %s\n", optArg[2]);
					exitStatus = 1;
					break;
                }

                if(fd[in] == -1 || fd[out] == -1 || fd[err] == -1) {
                    fprintf(stderr, "Error, file already closed\n");
                    exitStatus = 1;
                    break;
                }
                pid[subC] = fork();// Keep track of all the child process id numbers to be waited on at the end
				if (pid[subC] == -1) // Failed process fork
					fprintf(stderr,"System call fork failed: %s\n", strerror(errno));
                else if(pid[subC] == 0) { // Child
                    // close(STDIN_FILENO);
                    if(dup2(fd[in], STDIN_FILENO) == -1) { // In dup2
                        fprintf(stderr, "System call dup2 failed on logical file descriptor %d: %s\n", in, strerror(errno));
                        exit(1);
                    }
                    // close(STDOUT_FILENO);
                    if(dup2(fd[out], STDOUT_FILENO) == -1) { // Out dup2
                        fprintf(stderr, "System call dup2 failed on logical file descriptor %d: %s\n", out, strerror(errno));
                        exit(1);
                    }
                    // close(STDERR_FILENO);
                    if(dup2(fd[err], STDERR_FILENO) == -1) { // Err dup2
                        fprintf(stderr, "System call dup2 failed on logical file descriptor %d: %s\n", err, strerror(errno));
                        exit(1);
                    }
                    close(fd[in]);
                    close(fd[out]);
                    close(fd[err]);

                    fd[in] = -1;
                    fd[out] = -1;
                    fd[err] = -1;

                    for(i = 0; i < argc * 2; i++) {
                        if(fd[i] != -1)
                            close(fd[i]);
                    }

                    if (execvp(optArg[3], optArg+3) == -1) {
						fprintf(stderr,"System call execvp failed: %s\n", strerror(errno));
						exitStatus = 1;
					}
                    exit(exitStatus);
                }
                else { // Parent thread
                    fflush(stdout);
                    // Concatenate string with format "--command optarg1 optarg2"
					asprintf(&commandStats[subC], "%s %s\n", long_options[option_index].name, tempBuff);
                    asprintf(&commandStats2[subC], "%s\n", tempBuff2);
					subC++; 
                }
                if(proFlag == 1) {
                    if(getrusage(RUSAGE_SELF, &usage) == -1)
                            fprintf(stderr, "Error getting usage. %s\n", strerror(errno));
                    uEndTime = usage.ru_utime;
                    kEndTime = usage.ru_stime;
                    totalUser += ((double)uEndTime.tv_sec - (double)uStartTime.tv_sec) + (((double)uEndTime.tv_usec - (double)uStartTime.tv_usec)/1000000);
                    totalKernel += ((double)kEndTime.tv_sec - (double)kStartTime.tv_sec) + (((double)kEndTime.tv_usec - (double)kStartTime.tv_usec)/1000000);
                    fprintf(stdout, "%s: user: %f, kernel: %f, total: %f\n", strStat, 
                                totalUser, totalKernel, totalUser + totalKernel);
                }
                break;
            case 'q': // profile
                proFlag = 1;
                break;
            case 'P': // pause
                pause();
                if(proFlag == 1) {
                    if(getrusage(RUSAGE_SELF, &usage) == -1)
                            fprintf(stderr, "Error getting usage. %s\n", strerror(errno));
                    uEndTime = usage.ru_utime;
                    kEndTime = usage.ru_stime;
                    totalUser += ((double)uEndTime.tv_sec - (double)uStartTime.tv_sec) + (((double)uEndTime.tv_usec - (double)uStartTime.tv_usec)/1000000);
                    totalKernel += ((double)kEndTime.tv_sec - (double)kStartTime.tv_sec) + (((double)kEndTime.tv_usec - (double)kStartTime.tv_usec)/1000000);
                    fprintf(stdout, "%s: user: %f, kernel: %f, total: %f\n", strStat, 
                                totalUser, totalKernel, totalUser + totalKernel);
                }
                break;
            case 'v': // verbose
                verboseAct = 1;
                break;
            case 'H': // chdir
                if(chdir(optarg) == -1) {
                    fprintf(stderr, "chdir failed. Reason:%s\n", strerror(errno));
                    exitStatus = 1;
                }
                if(proFlag == 1) {
                    if(getrusage(RUSAGE_SELF, &usage) == -1)
                            fprintf(stderr, "Error getting usage. %s\n", strerror(errno));
                    uEndTime = usage.ru_utime;
                    kEndTime = usage.ru_stime;
                    totalUser += ((double)uEndTime.tv_sec - (double)uStartTime.tv_sec) + (((double)uEndTime.tv_usec - (double)uStartTime.tv_usec)/1000000);
                    totalKernel += ((double)kEndTime.tv_sec - (double)kStartTime.tv_sec) + (((double)kEndTime.tv_usec - (double)kStartTime.tv_usec)/1000000);
                    fprintf(stdout, "%s: user: %f, kernel: %f, total: %f\n", strStat, 
                                totalUser, totalKernel, totalUser + totalKernel);
                }
                break;
            case 'd': // default
                signal(atoi(optarg), SIG_DFL);
                if(proFlag == 1) {
                    if(getrusage(RUSAGE_SELF, &usage) == -1)
                            fprintf(stderr, "Error getting usage. %s\n", strerror(errno));
                    uEndTime = usage.ru_utime;
                    kEndTime = usage.ru_stime;
                    totalUser += ((double)uEndTime.tv_sec - (double)uStartTime.tv_sec) + (((double)uEndTime.tv_usec - (double)uStartTime.tv_usec)/1000000);
                    totalKernel += ((double)kEndTime.tv_sec - (double)kStartTime.tv_sec) + (((double)kEndTime.tv_usec - (double)kStartTime.tv_usec)/1000000);
                    fprintf(stdout, "%s: user: %f, kernel: %f, total: %f\n", strStat, 
                                totalUser, totalKernel, totalUser + totalKernel);
                }
                break;
            default:
                fprintf(stderr, "Unrecognized argument. Usage: --rdonly --wronly --command --verbose only\n");
                exit(1);
                break;
        }
        fflush(stdout);
        free(strStat);
    } // End of get_opt :(

    for (i=0; i < len; i++) {
        if(fd[i] != -1) {
            close(fd[i]);
            fd[i] = -1;
        }
    }

    if(sigExit > 0) {
        signal(sigExit, SIG_DFL);
        raise(sigExit); 
    }
    exit(exitStatus);
}
