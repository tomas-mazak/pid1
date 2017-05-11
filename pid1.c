/*
 * Simple PID1 implementation for light docker containers.
 * Reaps zombies and launches the main container process.
 *
 * Based on Rich Felker's proposal for correct PID1 process: http://ewontfix.com
 *
 * Tomas Mazak <tomas.mazak@rackspace.co.uk>
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/wait.h>


#define TERM_ATTEMPTS 5


#define log_error(...) fprintf(stderr, __VA_ARGS__)
#ifdef DEBUG
#define log_debug(...) printf(__VA_ARGS__)
#else
#define log_debug(...)
#endif


/**
 * When forking to start the actual main process, store its PID into this
 * variable.  
 */
static int pid2 = 0;
static int pid2_exitcode = 255;


/**
 * Find a parent process for the given PID using /proc filesystem
 */
static int get_ppid(char *pid)
{
    char fname[18];
    FILE *fd;
    int ppid;

    snprintf(fname, 18, "/proc/%s/stat", pid);
    if((fd = fopen(fname, "r")) == NULL) {
        return 0;
    }
    if(fscanf(fd, "%*d %*s %*c %d", &ppid) != 1) {
        return 0;
    }
    fclose(fd);

    return ppid;
}


/**
 * Send a TERM signal to all children of the given process
 */
static int kill_children(int pid)
{
    DIR *dd;
    struct dirent *item;
    int killed = 0;
    
    if((dd = opendir("/proc")) == NULL) 
        return 0;

    while((item = readdir(dd)) != NULL) {
        if(!isdigit(item->d_name[0]))
            continue;
        
        if(get_ppid(item->d_name) == pid) {
            log_debug("Killing %s\n", item->d_name);
            killed++;
            kill(atoi(item->d_name), SIGTERM);
        }
    }

    closedir(dd);
    return killed;
}


/**
 * Reap zombies in loop until we don't have any children left
 */
static void reap_zombies(void)
{
    int w, status;

    do { 
        w = wait(&status);
        log_debug("Child with PID %d terminated and reaped\n", w);
        if(w == pid2 && WIFEXITED(status))
            pid2_exitcode = WEXITSTATUS(status);
        if(w == pid2 && WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM)
            pid2_exitcode = 0;
    } while(w != -1 || errno != ECHILD);
}


/**
 * On SIGTERM, propagate the signal to all children and reap them before
 * exiting. Because this action can cause other processes to be orphaned and
 * adopted by us, repeat this action several times, so we send the TERM signal
 * to the new orphans as well
 */
static void terminate()
{
    int i;

    log_debug("SIGTERM received.\n");
    for(i=0; i<TERM_ATTEMPTS; i++) {
        log_debug("Kill attempt %d ...\n", i+1);
        if(kill_children(getpid()) == 0)
            break;
        sleep(1);
        reap_zombies();
    }

    exit(pid2_exitcode);
}


/**
 * Start the actual main program as PID2 and continue reaping zombies until
 * we don't have any children left
 */
int main(int argc, char *argv[])
{
    sigset_t set;

    if(argc < 2) {
        log_error("USAGE: %s <command> [arg1 [arg2 ... [argN]]]\n", argv[0]);
        return 1;
    }

#ifndef DEBUG
    if (getpid() != 1) {
        log_error("I can be run as PID 1 only, exiting...\n");
        return 1;
    }
#endif

    if( (pid2 = fork()) ) {
        log_debug("PID1: pid %d; Forked: pid %d\n", getpid(), pid2);
        sigfillset(&set);
        sigdelset(&set, SIGTERM);
        if(sigprocmask(SIG_SETMASK, &set, 0) != 0) {
            log_error("Unable to mask signals, exiting...\n");
            return 1;
        }

        if (signal(SIGTERM, terminate) == SIG_ERR) {
            log_error("Unable to set SIGTERM handler, exiting...\n");
            return 1;
        }

        reap_zombies();

        return pid2_exitcode;
    }

    setsid();
    setpgid(0, 0);
    return execvp(argv[1], &argv[1]);
}
