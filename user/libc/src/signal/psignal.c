#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

void psignal(int sig, const char *s) {
    int saved = errno;
    if (s && *s) {
        fprintf(stderr, "%s: %s\n", s, strsignal(sig));
    } else {
        fprintf(stderr, "%s\n", strsignal(sig));
    }
    errno = saved;
}

void psiginfo(const siginfo_t *info, const char *s) {
    psignal(info->si_signo, s);
}

static const char *const names[] = {
    nullptr,  "HUP",  "INT",    "QUIT", "ILL",   "TRAP", "ABRT", "BUS",
    "FPE",    "KILL", "USR1",   "SEGV", "USR2",  "PIPE", "ALRM", "TERM",
    "STKFLT", "CHLD", "CONT",   "STOP", "TSTP",  "TTIN", "TTOU", "URG",
    "XCPU",   "XFSZ", "VTALRM", "PROF", "WINCH", "POLL", "PWR",  "SYS",

};

#define NNAMES (int)(sizeof(names) / sizeof(names[0]))

int sig2str(int sig, char *str) {
    if (sig > 0 && sig < NNAMES) {
        strcpy(str, names[sig]);
    } else if (sig == SIGRTMIN) {
        strcpy(str, "RTMIN");
    } else if (sig == SIGRTMAX) {
        strcpy(str, "RTMAX");
    } else if (sig > SIGRTMIN && sig <= (SIGRTMIN + SIGRTMAX) / 2) {
        snprintf(str, SIG2STR_MAX, "RTMIN+%d", sig - SIGRTMIN);
    } else if (sig > SIGRTMIN && sig < SIGRTMAX) {
        snprintf(str, SIG2STR_MAX, "RTMAX-%d", SIGRTMAX - sig);
    } else {
        return -1;
    }
    return 0;
}

static int number(const char *s, int *out) {
    int n = 0;
    if (!*s) {
        return -1;
    }
    for (; *s; s++) {
        if (*s < '0' || *s > '9' || n > NSIG) {
            return -1;
        }
        n = n * 10 + (*s - '0');
    }
    *out = n;
    return 0;
}

int str2sig(const char *__restrict str, int *__restrict sig) {
    int n;
    for (int i = 1; i < NNAMES; i++) {
        if (!strcmp(str, names[i])) {
            *sig = i;
            return 0;
        }
    }
    if (!strcmp(str, "RTMIN") || !strcmp(str, "RTMAX")) {
        *sig = str[3] == 'I' ? SIGRTMIN : SIGRTMAX;
        return 0;
    }
    if (!strncmp(str, "RTMIN+", 6) && !number(str + 6, &n) &&
        SIGRTMIN + n <= SIGRTMAX) {
        *sig = SIGRTMIN + n;
        return 0;
    }
    if (!strncmp(str, "RTMAX-", 6) && !number(str + 6, &n) &&
        SIGRTMAX - n >= SIGRTMIN) {
        *sig = SIGRTMAX - n;
        return 0;
    }
    if (!number(str, &n) && n > 0 && n < NSIG) {
        *sig = n;
        return 0;
    }
    return -1;
}
