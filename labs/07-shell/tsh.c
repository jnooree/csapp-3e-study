/*
 * tsh - A tiny shell program with job control
 *
 * <Put your name and login ID here>
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Misc manifest constants */
#define MAXLINE 1024    /* max line size */
#define MAXARGS 128     /* max args on a command line */
#define MAXJOBS 16      /* max jobs at any point in time */
#define MAXJID  1 << 16 /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG    1 /* running in foreground */
#define BG    2 /* running in background */
#define ST    3 /* stopped */

/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
static char prompt[] = "tsh> "; /* command line prompt (DO NOT CHANGE) */
static int verbose = 0;         /* if true, print additional output */
static int nextjid = 1;         /* next job ID to allocate */

struct job_t {           /* The job struct */
  pid_t pid;             /* job PID */
  int jid;               /* job ID [1, 2, ...] */
  int state;             /* UNDEF, BG, FG, or ST */
  char cmdline[MAXLINE]; /* command line */
};
static struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */

/* Function prototypes */

/* Here are the functions that you will implement */
static void eval(char *cmdline);
static int builtin_cmd(char **argv);
static void do_bgfg(char **argv);
static void waitfg(pid_t pid);
static void infobg(struct job_t *job);

static void block_sigchld(sigset_t *oldset);
static void restore_signal(sigset_t *oldset);

static void sigchld_handler(int sig);
static void reap_child(pid_t pid, int status);
static void wait_childs(void);
static void sigint_tstp_handler(int sig);

/* Here are helper routines that we've provided for you */
static int parseline(const char *cmdline, char **argv);
static void sigquit_handler(int sig) __attribute__((noreturn));

static void clearjob(struct job_t *job);
static void initjobs(void);
static void destructjob(struct job_t *job);
static void do_quit(void) __attribute__((noreturn));
static struct job_t *maxjob(void);
static struct job_t *addjob(pid_t pid, int state, char *cmdline);
static void deletejob(struct job_t *job);
static struct job_t *getjobfg(void);
static struct job_t *getjobpid(pid_t pid);
static struct job_t *getjobjid(int jid);
static void listjobs(void);

static void usage(void) __attribute__((noreturn));
static void unix_error(char *msg) __attribute__((noreturn));
static void app_error(char *msg) __attribute__((noreturn));

typedef void handler_t(int);
static handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine
 */
int main(int argc, char **argv) {
  char c;
  char cmdline[MAXLINE];
  int emit_prompt = 1; /* emit prompt (default) */

  /* Redirect stderr to stdout (so that driver will get all output
   * on the pipe connected to stdout) */
  dup2(1, 2);

  /* Parse the command line */
  while ((c = getopt(argc, argv, "hvp")) != EOF) {
    switch (c) {
    case 'h': /* print help message */
      usage();
    case 'v': /* emit additional diagnostic info */
      verbose = 1;
      break;
    case 'p':          /* don't print a prompt */
      emit_prompt = 0; /* handy for automatic testing */
      break;
    default:
      usage();
    }
  }

  /* Install the signal handlers */

  /* These are the ones you will need to implement */
  Signal(SIGINT, sigint_tstp_handler);  /* ctrl-c */
  Signal(SIGTSTP, sigint_tstp_handler); /* ctrl-z */
  Signal(SIGCHLD, sigchld_handler);     /* Terminated or stopped child */

  /* This one provides a clean way to kill the shell */
  Signal(SIGQUIT, sigquit_handler);

  /* Initialize the job list */
  initjobs();

  /* Execute the shell's read/eval loop */
  while (1) {
    /* Read command line */
    if (emit_prompt) {
      printf("%s", prompt);
      fflush(stdout);
    }
    if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
      app_error("fgets error");
    if (feof(stdin)) { /* End of file (ctrl-d) */
      fflush(stdout);
      do_quit();
    }

    /* Evaluate the command line */
    eval(cmdline);
    fflush(stderr);
    fflush(stdout);
  }
}

/*
 * eval - Evaluate the command line that the user has just typed in
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
 */
void eval(char *cmdline) {
  static char *argv[MAXARGS + 1];
  struct job_t *job;
  sigset_t oldset;

  int bg, builtin;
  pid_t pid;

  bg = parseline(cmdline, argv);
  if (argv[0] == NULL)
    return;

  builtin = builtin_cmd(argv);
  if (builtin)
    return;

  block_sigchld(&oldset);

  pid = fork();
  if (pid < 0) {
    perror("fork");
    restore_signal(&oldset);
    return;
  }

  if (pid == 0) {
    setpgid(0, 0);
    restore_signal(&oldset);
    if (execvp(argv[0], argv))
      unix_error(argv[0]);
  }

  job = addjob(pid, bg ? BG : FG, cmdline);
  if (bg)
    infobg(job);

  restore_signal(&oldset);

  if (!bg)
    waitfg(pid);
}

/*
 * parseline - Parse the command line and build the argv array.
 *
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.
 */
int parseline(const char *cmdline, char **argv) {
  static char array[MAXLINE]; /* holds local copy of command line */
  char *buf = array;          /* ptr that traverses command line */
  char *delim;                /* points to first space delimiter */
  int argc;                   /* number of args */
  int bg;                     /* background job? */

  strcpy(buf, cmdline);
  buf[strlen(buf) - 1] = ' ';   /* replace trailing '\n' with space */
  while (*buf && (*buf == ' ')) /* ignore leading spaces */
    buf++;

  /* Build the argv list */
  argc = 0;
  if (*buf == '\'') {
    buf++;
    delim = strchr(buf, '\'');
  } else {
    delim = strchr(buf, ' ');
  }

  while (delim) {
    argv[argc++] = buf;
    *delim = '\0';
    buf = delim + 1;
    while (*buf && (*buf == ' ')) /* ignore spaces */
      buf++;

    if (*buf == '\'') {
      buf++;
      delim = strchr(buf, '\'');
    } else {
      delim = strchr(buf, ' ');
    }
  }
  argv[argc] = NULL;

  if (argc == 0) /* ignore blank line */
    return 1;

  /* should the job run in the background? */
  bg = *argv[argc - 1] == '&';
  if (bg != 0)
    argv[--argc] = NULL;
  return bg;
}

/*
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.
 */
int builtin_cmd(char **argv) {
  const char *prgnam = argv[0];
  if (strcmp(prgnam, "fg") == 0 || strcmp(prgnam, "bg") == 0) {
    do_bgfg(argv);
    return 1;
  }
  if (strcmp(prgnam, "jobs") == 0) {
    listjobs();
    return 1;
  }
  if (strcmp(prgnam, "quit") == 0)
    do_quit();

  return 0; /* not a builtin command */
}

/*
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) {
  static const char *const fmt_str[] = {
    "(%s): No such process\n",
    "%s: No such job\n",
  };

  sigset_t oldset;
  struct job_t *job;
  char *id_ptr, *endptr;
  int bg, id, arg_jid;
  pid_t pid;

  if (argv[1] == NULL) {
    printf("%s command requires PID or %%jobid argument\n", argv[0]);
    return;
  }

  arg_jid = argv[1][0] == '%';
  id_ptr = argv[1] + arg_jid;

  errno = 0;
  id = strtol(id_ptr, &endptr, 10);
  if (id_ptr == endptr || *endptr != '\0' || errno) {
    printf("%s: argument must be a PID or %%jobid\n", argv[0]);
    restore_signal(&oldset);
    return;
  }

  block_sigchld(&oldset);

  job = (arg_jid ? getjobjid : getjobpid)(id);
  if (job == NULL) {
    printf(fmt_str[arg_jid], argv[1]);
    restore_signal(&oldset);
    return;
  }

  bg = argv[0][0] == 'b';

  pid = job->pid;
  job->state = bg ? BG : FG;
  killpg(job->pid, SIGCONT);
  if (bg)
    infobg(job);

  restore_signal(&oldset);

  if (!bg)
    waitfg(pid);
}

/*
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid) {
  int ret, status;
  sigset_t oldset;

  ret = waitpid(pid, &status, WUNTRACED);
  if (ret == -1) {
    if (errno != ECHILD)
      unix_error("waitfg");
    return;
  }

  block_sigchld(&oldset);

  reap_child(pid, status);
  wait_childs();

  restore_signal(&oldset);
}

void infobg(struct job_t *job) {
  printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
}

/*****************
 * Signal handlers
 *****************/
void block_sigchld(sigset_t *oldset) {
  sigset_t new_set;

  sigemptyset(&new_set);
  sigaddset(&new_set, SIGCHLD);
  sigprocmask(SIG_BLOCK, &new_set, oldset);
}

void restore_signal(sigset_t *oldset) {
  sigprocmask(SIG_SETMASK, oldset, NULL);
}

void reap_child(pid_t pid, int status) {
  struct job_t *job;

  job = getjobpid(pid);
  if (job == NULL)
    return;

  if (WIFSTOPPED(status)) {
    printf("Job [%d] (%d) stopped by signal %d\n", job->jid, job->pid,
           WSTOPSIG(status));

    job->state = ST;
    return;
  }

  if (WIFSIGNALED(status))
    printf("Job [%d] (%d) terminated by signal %d\n", job->jid, job->pid,
           WTERMSIG(status));

  deletejob(job);
}

void wait_childs(void) {
  int status;
  pid_t pid;

  while ((pid = waitpid(WAIT_ANY, &status, WNOHANG | WUNTRACED)) != 0) {
    if (pid == -1) {
      if (errno != ECHILD)
        perror("wait_childs");
      break;
    }

    reap_child(pid, status);
  }
}

/*
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.
 */
void sigchld_handler(__attribute__((unused)) int sig) {
  int errno_save = errno;
  wait_childs();
  errno = errno_save;
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 */
void sigint_tstp_handler(int sig) {
  struct job_t *job;
  sigset_t oldset;

  block_sigchld(&oldset);

  job = getjobfg();
  if (job == NULL)
    goto restore;

  killpg(job->pid, sig);

restore:
  restore_signal(&oldset);
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
  job->pid = 0;
  job->jid = 0;
  job->state = UNDEF;
  job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(void) {
  int i;

  for (i = 0; i < MAXJOBS; i++)
    clearjob(&jobs[i]);
}

void destructjob(struct job_t *job) {
  if (job->pid == 0)
    return;

  killpg(job->pid, SIGHUP);
}

void do_quit(void) {
  sigset_t oldset;
  int i;

  block_sigchld(&oldset);

  for (i = 0; i < MAXJOBS; i++)
    destructjob(&jobs[i]);

  exit(0);
}

/* maxjid - Returns largest allocated job ID */
struct job_t *maxjob(void) {
  struct job_t *job = NULL;
  int i, max = 0;

  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid > max) {
      job = &jobs[i];
      max = job->jid;
    }

  return job;
}

/* addjob - Add a job to the job list */
struct job_t *addjob(pid_t pid, int state, char *cmdline) {
  struct job_t *job = getjobpid(0);
  if (job == NULL) {
    printf("Tried to create too many jobs\n");
    return NULL;
  }

  job->pid = pid;
  job->state = state;
  job->jid = nextjid++;
  if (nextjid > MAXJOBS)
    nextjid = 1;
  strcpy(job->cmdline, cmdline);
  if (verbose)
    printf("Added job [%d] %d %s\n", job->jid, job->pid, job->cmdline);
  return job;
}

/* deletejob - Delete a job whose PID=pid from the job list */
void deletejob(struct job_t *job) {
  struct job_t *max_job;

  clearjob(job);

  max_job = maxjob();
  nextjid = max_job == NULL ? 1 : max_job->jid + 1;
}

/* fgpid - Return PID of current foreground job, NULL if no such job */
struct job_t *getjobfg(void) {
  int i;

  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].state == FG)
      return &jobs[i];
  return NULL;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(pid_t pid) {
  int i;

  if (pid < 0)
    return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].pid == pid)
      return &jobs[i];
  return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(int jid) {
  int i;

  if (jid < 1)
    return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid == jid)
      return &jobs[i];
  return NULL;
}

/* listjobs - Print the job list */
void listjobs(void) {
  int i;
  sigset_t oldset;

  block_sigchld(&oldset);

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid != 0) {
      printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
      switch (jobs[i].state) {
      case BG:
        printf("Running ");
        break;
      case FG:
        printf("Foreground ");
        break;
      case ST:
        printf("Stopped ");
        break;
      default:
        printf("listjobs: Internal error: job[%d].state=%d ", i, jobs[i].state);
      }
      printf("%s", jobs[i].cmdline);
    }
  }

  restore_signal(&oldset);
}

/******************************
 * end job list helper routines
 ******************************/

/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) {
  printf("Usage: shell [-hvp]\n");
  printf("   -h   print this message\n");
  printf("   -v   print additional diagnostic information\n");
  printf("   -p   do not emit a command prompt\n");
  exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg) {
  printf("%s: %s\n", msg,
         errno == ENOENT ? "Command not found" : strerror(errno));
  exit(-errno);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg) {
  printf("%s\n", msg);
  exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) {
  struct sigaction action, old_action;

  action.sa_handler = handler;
  sigemptyset(&action.sa_mask); /* block sigs of type being handled */
  action.sa_flags = SA_RESTART; /* restart syscalls if possible */

  if (sigaction(signum, &action, &old_action) < 0)
    unix_error("Signal error");
  return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(__attribute__((unused)) int sig) {
  printf("Terminating after receipt of SIGQUIT signal\n");
  exit(1);
}
