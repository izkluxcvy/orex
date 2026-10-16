#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <ktime.h>
#include <printf.h>
#include <proc.h>
#include <sched.h>
#include <signal.h>
#include <spinlock.h>
#include <tty.h>
#include <uaccess.h>
#include <vfs.h>
#include <waitq.h>

#define CTRL(c) ((c) & 0x1f)

static struct tty console;

static void wake(struct waitq *wq) { waitq_wakeup_all(wq); }

void console_size(int *rows, int *cols);

void serial_putc(char c);

static size_t console_output(struct tty *t, const char *p, size_t n) {
    (void)t;
    struct tty *s = tty_serial();
    if (s && s->session) {
        printf_write_raw_except(serial_putc, p, n);
    } else {
        printf_write_raw(p, n);
    }
    return n;
}

void tty_init(struct tty *t,
              size_t (*output)(struct tty *, const char *, size_t)) {
    *t = (struct tty){
        .t      = {.c_iflag = ICRNL | IXON,
                   .c_oflag = OPOST | ONLCR,
                   .c_cflag = B38400 | CS8 | CREAD | HUPCL,
                   .c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL |
                              ECHOKE | IEXTEN,
                   .c_cc    = {[VINTR]    = CTRL('C'),
                               [VQUIT]    = CTRL('\\'),
                               [VERASE]   = 0x7f,
                               [VKILL]    = CTRL('U'),
                               [VEOF]     = CTRL('D'),
                               [VMIN]     = 1,
                               [VSTART]   = CTRL('Q'),
                               [VSTOP]    = CTRL('S'),
                               [VSUSP]    = CTRL('Z'),
                               [VREPRINT] = CTRL('R'),
                               [VWERASE]  = CTRL('W'),
                               [VLNEXT]   = CTRL('V')}},
        .ws     = {.ws_row = 24, .ws_col = 80},
        .output = output,
    };
    waitq_init(&t->readers);
    waitq_init(&t->writers);
}

static int on_serial;

void tty_console_select(const char *name) {
    on_serial =
        name[0] == 't' && name[1] == 't' && name[2] == 'y' && name[3] == 'S';
}

struct tty *tty_console() {
    return on_serial && tty_serial() ? tty_serial() : tty_fb();
}

struct tty *tty_fb() {
    if (!console.output) {
        tty_init(&console, console_output);
        int r, c;
        console_size(&r, &c);
        if (r && c) {
            console.ws = (struct winsize){.ws_row = (unsigned short)r,
                                          .ws_col = (unsigned short)c};
        }
    }
    return &console;
}

// --- Output ------------------------------------------------------------------

static size_t post(struct tty *t, char c, char out[2]) {
    uint32_t o = t->t.c_oflag;
    if (!(o & OPOST)) {
        out[0] = c;
        return 1;
    }
    if (c == '\n' && (o & ONLCR)) {
        out[0] = '\r', out[1] = '\n';
        return 2;
    }
    out[0] = c;
    return 1;
}

static void echo(struct tty *t, const char *p, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char   b[2];
        size_t k = post(t, p[i], b);
        t->output(t, b, k);
    }
}

static int is_ctl(unsigned char c) { return c < ' ' || c == 0x7f; }

static void echo_char(struct tty *t, unsigned char c) {
    if (!(t->t.c_lflag & ECHO)) {
        return;
    }
    if ((t->t.c_lflag & ECHOCTL) && is_ctl(c) && c != '\t' && c != '\n') {
        char ctl[2] = {'^', c == 0x7f ? '?' : (char)(c + '@')};
        echo(t, ctl, 2);
    } else {
        echo(t, (const char *)&c, 1);
    }
}

// --- Input -------------------------------------------------------------------

static int is(struct tty *t, int idx, unsigned char c) {
    return t->t.c_cc[idx] && t->t.c_cc[idx] == c;
}

static void in_put(struct tty *t, char c) {
    if (t->in_len < TTY_BUF) {
        t->in[(t->in_head + t->in_len++) % TTY_BUF] = c;
    }
}

static void flush_input(struct tty *t) {
    t->line_len = t->in_len = t->eol_count = 0;
}

static void commit(struct tty *t) {
    size_t n = 0;
    for (; n < t->line_len && t->in_len < TTY_BUF; n++) {
        in_put(t, t->line[n]);
    }
    t->line_len = 0;
    if (t->eol_count < TTY_EOLS) {
        t->eol[(t->eol_head + t->eol_count++) % TTY_EOLS] = n;
    }
    wake(&t->readers);
}

static void erase(struct tty *t) {
    if (!t->line_len) {
        return;
    }
    unsigned char c = (unsigned char)t->line[--t->line_len];
    if ((t->t.c_lflag & ECHO) && (t->t.c_lflag & ECHOE)) {
        int width = (t->t.c_lflag & ECHOCTL) && is_ctl(c) && c != '\t' ? 2 : 1;
        for (int i = 0; i < width; i++) {
            echo(t, "\b \b", 3);
        }
    }
}

static void signal_fg(struct tty *t, int sig) {
    if (t->pgrp && (sig != SIGTSTP || !pgrp_orphaned_locked(t->pgrp))) {
        signal_group_locked(t->pgrp, sig);
    }
}

void tty_input_locked(struct tty *t, char ch) {
    unsigned char c = (unsigned char)ch;
    uint32_t      i = t->t.c_iflag;
    uint32_t      l = t->t.c_lflag;
    if (i & ISTRIP) {
        c &= 0x7f;
    }
    if (t->lnext) {
        t->lnext = 0;
        goto plain;
    }
    if (c == '\r') {
        if (i & IGNCR) {
            return;
        }
        if (i & ICRNL) {
            c = '\n';
        }
    } else if (c == '\n' && (i & INLCR)) {
        c = '\r';
    }
    if (l & ISIG) {
        int sig = is(t, VINTR, c)   ? SIGINT
                  : is(t, VQUIT, c) ? SIGQUIT
                  : is(t, VSUSP, c) ? SIGTSTP
                                    : 0;
        if (sig) {
            if (!(l & NOFLSH)) {
                flush_input(t);
            }
            echo_char(t, c);
            if (l & ECHO) {
                echo(t, "\n", 1);
            }
            signal_fg(t, sig);
            return;
        }
    }
    if (i & IXON) {
        if (is(t, VSTOP, c)) {
            t->out_stopped = 1;
            return;
        }
        if (is(t, VSTART, c)) {
            t->out_stopped = 0;
            wake(&t->writers);
            return;
        }
    }
    if ((i & IXANY) && t->out_stopped) {
        t->out_stopped = 0;
        wake(&t->writers);
    }
    if (l & IEXTEN) {
        if (is(t, VLNEXT, c)) {
            t->lnext = 1;
            return;
        }
    }
    if (l & ICANON) {
        if (is(t, VERASE, c)) {
            erase(t);
            return;
        }
        if ((l & IEXTEN) && is(t, VWERASE, c)) {
            while (t->line_len && t->line[t->line_len - 1] == ' ') {
                erase(t);
            }
            while (t->line_len && t->line[t->line_len - 1] != 0) {
                erase(t);
            }
            return;
        }
        if (is(t, VKILL, c)) {
            if ((l & ECHOE) && (l & ECHOKE)) {
                while (t->line_len) {
                    erase(t);
                }
            } else {
                t->line_len = 0;
                echo_char(t, c);
                if (l & ECHOK) {
                    echo(t, "\n", 1);
                }
            }
            return;
        }
        if ((l & IEXTEN) && is(t, VREPRINT, c)) {
            echo_char(t, c);
            echo(t, "\n", 1);
            echo(t, t->line, t->line_len);
            return;
        }
        if (is(t, VEOF, c)) {
            commit(t);
            return;
        }
        if (c == '\n' || is(t, VEOL, c) || is(t, VEOL2, c)) {
            if (t->line_len < TTY_LINE) {
                t->line[t->line_len++] = (char)c;
            }
            if ((l & ECHO) || (c == '\n' && (l & ECHONL))) {
                echo(t, (const char *)&c, 1);
            }
            commit(t);
            return;
        }
    }

plain:
    if (l & ICANON) {
        if (t->line_len < TTY_LINE - 1) {
            t->line[t->line_len++] = (char)c;
            echo_char(t, c);
        }
        return;
    }
    in_put(t, (char)c);
    echo_char(t, c);
    wake(&t->readers);
}

void tty_input(struct tty *t, char c) {
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    tty_input_locked(t, c);
    spin_unlock_irqrestore(&sched_lock, flags);
}

// --- Job control -------------------------------------------------------------

static long job_check(struct tty *t, int sig) {
    struct proc *p = curproc();
    if (p->ctty != t || !t->pgrp || p->pgid == t->pgrp) {
        return 0;
    }
    if (signal_ignored_or_blocked(p, sig)) {
        return sig == SIGTTIN ? -EIO : 0;
    }
    if (pgrp_orphaned_locked(p->pgid)) {
        return -EIO;
    }
    signal_group_locked(p->pgid, sig);
    return -ERESTARTSYS;
}

static void acquire(struct tty *t, struct proc *p) {
    t->session = p->sid;
    t->pgrp    = p->pgid;
    p->ctty    = t;
}

void tty_opened(struct tty *t, int flags) {
    struct proc *p     = curproc();
    uint64_t     lflag = spin_lock_irqsave(&sched_lock);
    if (!(flags & O_NOCTTY) && p->sid == p->pid && !p->ctty && !t->session &&
        !t->hungup) {
        acquire(t, p);
    }
    spin_unlock_irqrestore(&sched_lock, lflag);
}

static void disassociate(struct tty *t) {
    if (t->pgrp) {
        signal_group_locked(t->pgrp, SIGHUP);
        signal_group_locked(t->pgrp, SIGCONT);
    }
    for (struct proc *q = proc_list; q; q = q->all_next) {
        if (q->ctty == t) {
            q->ctty = nullptr;
        }
    }
    t->session = t->pgrp = 0;
}

void tty_session_ended_locked(struct proc *p) {
    if (p->ctty && p->ctty->session == p->sid) {
        disassociate(p->ctty);
    }
}

void tty_hangup_locked(struct tty *t) {
    t->hungup = 1;
    if (t->session) {
        struct proc *leader = proc_find_locked(t->session);
        if (leader) {
            signal_send_locked(leader, SIGHUP);
        }
        disassociate(t);
    }
    wake(&t->readers);
    wake(&t->writers);
}

// --- Read and write ----------------------------------------------------------

static size_t take(struct tty *t, char *out, size_t n) {
    size_t k = 0;
    for (; k < n && t->in_len; k++) {
        out[k]     = t->in[t->in_head];
        t->in_head = (t->in_head + 1) % TTY_BUF;
        t->in_len--;
    }
    return k;
}

static long wait_input(struct tty *t, int nonblock, uint64_t deadline) {
    if (t->hungup) {
        return 1;
    }
    if (nonblock) {
        return -EAGAIN;
    }
    int r = deadline ? sleep_until(&t->readers, deadline)
                     : waitq_sleep_intr(&t->readers);
    return r == -EINTR ? -ERESTARTSYS : r == -ETIMEDOUT ? 2 : 0;
}

long tty_read(struct tty *t, void *ubuf, size_t n, int nonblock) {
    char buf[TTY_BUF];
    if (n > sizeof(buf)) {
        n = sizeof(buf);
    }
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    long     ret   = 0;
    size_t   got   = 0;

    while (1) {
        if ((ret = job_check(t, SIGTTIN)) != 0) {
            goto out;
        }
        if (t->t.c_lflag & ICANON) {
            if (t->eol_count) {
                size_t *len = &t->eol[t->eol_head];
                got         = take(t, buf, n < *len ? n : *len);
                *len -= got;
                if (!*len) {
                    t->eol_head = (t->eol_head + 1) % TTY_EOLS;
                    t->eol_count--;
                }
                break;
            }
            if ((ret = wait_input(t, nonblock, 0)) != 0) {
                ret = ret == 1 ? 0 : ret;
                goto out;
            }
            continue;
        }

        size_t   min  = t->t.c_cc[VMIN];
        uint64_t time = (uint64_t)t->t.c_cc[VTIME] * 100'000'000;
        if ((!min && !time) || (min && t->in_len >= (min < n ? min : n)) ||
            (!min && t->in_len)) {
            got = take(t, buf, n);
            break;
        }
        uint64_t deadline =
            time && (!min || t->in_len) ? time_mono() + time : 0;
        long r = wait_input(t, nonblock, deadline);
        if (r == 1 || r == 2 || (r == -EAGAIN && t->in_len)) {
            got = take(t, buf, n);
            break;
        }
        if (r < 0) {
            ret = r;
            goto out;
        }
    }
    wake(&t->writers);
    spin_unlock_irqrestore(&sched_lock, flags);
    return copy_to_user(ubuf, buf, got) ? -EFAULT : (long)got;
out:
    spin_unlock_irqrestore(&sched_lock, flags);
    return ret;
}

static long write_chunk(struct tty *t, const char *p, size_t n, int nonblock) {
    uint64_t flags = spin_lock_irqsave(&sched_lock);
    size_t   done  = 0;
    long     ret   = 0;
    if (t->hungup) {
        ret = -EIO;
        goto out;
    }
    if ((t->t.c_lflag & TOSTOP) && (ret = job_check(t, SIGTTOU)) != 0) {
        goto out;
    }
    while (done < n) {
        char   b[2];
        size_t k   = post(t, p[done], b);
        size_t put = 0;
        while (put < k) {
            if (t->hungup) {
                ret = -EIO;
                goto out;
            }
            size_t a = t->out_stopped ? 0 : t->output(t, b + put, k - put);
            put += a;
            if (a) {
                continue;
            }
            if (nonblock) {
                return -EAGAIN;
                goto out;
            }
            if (waitq_sleep_intr(&t->writers) != 0) {
                ret = -ERESTARTSYS;
                goto out;
            }
        }
        done++;
    }

out:
    spin_unlock_irqrestore(&sched_lock, flags);
    return done ? (long)done : ret;
}

long tty_write(struct tty *t, const void *ubuf, size_t n, int nonblock) {
    char   chunk[256];
    size_t done = 0;
    long   ret  = 0;
    while (done < n) {
        size_t k = n - done < sizeof(chunk) ? n - done : sizeof(chunk);
        if (copy_from_user(chunk, (const char *)ubuf + done, k) != 0) {
            ret = -EFAULT;
            break;
        }
        long r = write_chunk(t, chunk, k, nonblock);
        if (r < 0) {
            ret = r;
            break;
        }
        done += (size_t)r;
        if ((size_t)r < k) {
            break;
        }
    }
    return done ? (long)done : ret;
}

// --- ioctl -------------------------------------------------------------------

static int in_session(struct tty *t, pid_t pgrp) {
    for (struct proc *q = proc_list; q; q = q->all_next) {
        if (q->pgid == pgrp && q->sid == t->session && q->state == PROC_ALIVE) {
            return 1;
        }
    }
    return 0;
}

static void set_termios(struct tty *t, const struct termios *nt, int flush) {
    int was_canon = t->t.c_lflag & ICANON;
    t->t          = *nt;
    if (flush) {
        flush_input(t);
    }
    if (was_canon && !(nt->c_lflag & ICANON)) {
        for (size_t i = 0; i < t->line_len; i++) {
            in_put(t, t->line[i]);
        }
        t->line_len = t->eol_count = 0;
    } else if (!was_canon && (nt->c_lflag & ICANON) && t->in_len) {
        t->eol[0]   = t->in_len;
        t->eol_head = 0, t->eol_count = 1;
    }
    wake(&t->readers);
}

long tty_ioctl(struct tty *t, struct file *f, unsigned long req, void *arg) {
    (void)f;
    struct proc   *p = curproc();
    struct termios nt;
    struct winsize ws;
    int            val = 0;
    long           ret = 0;
    uint64_t       flags;

    switch (req) {
    case TCSETS:
    case TCSETSW:
    case TCSETSF:
        if (copy_from_user(&nt, arg, sizeof(nt)) != 0) {
            return -EFAULT;
        }
        break;
    case TIOCSWINSZ:
        if (copy_from_user(&ws, arg, sizeof(ws)) != 0) {
            return -EFAULT;
        }
        break;
    case TIOCSPGRP:
        if (copy_from_user(&val, arg, sizeof(val)) != 0) {
            return -EFAULT;
        }
        break;
    }

    flags = spin_lock_irqsave(&sched_lock);
    switch (req) {
    case TCGETS:
        nt = t->t;
        break;
    case TCSETS:
    case TCSETSW:
    case TCSETSF:
    case TCSBRK:
    case TCXONC:
    case TCFLSH:
        if ((ret = job_check(t, SIGTTOU)) != 0) {
            break;
        }
        if (req == TCXONC) {
            long a = (long)arg;
            if (a == 0 || a == 1) {
                t->out_stopped = a == 0;
                wake(&t->writers);
            } else if (a == 2 || a == 3) {
                char c = (char)t->t.c_cc[a == 2 ? VSTOP : VSTART];
                t->output(t, &c, 1);
            } else {
                ret = -EINVAL;
            }
        } else if (req == TCFLSH) {
            long a = (long)arg;
            if (a < 0 || a > 2) {
                ret = -EINVAL;
            } else if (a != 1) {
                flush_input(t);
            }
        } else if (req != TCSBRK) {
            set_termios(t, &nt, req == TCSETSF);
        }
        break;
    case TIOCGPGRP:
    case TIOCGSID:
        if (p->ctty != t) {
            ret = -ENOTTY;
        } else if (req == TIOCGSID && !t->session) {
            ret = -ENOTTY;
        } else {
            val = req == TIOCGPGRP ? t->pgrp : t->session;
        }
        break;
    case TIOCSPGRP:
        if (p->ctty != t || !t->session) {
            return -ENOTTY;
        } else if ((ret = job_check(t, SIGTTOU)) != 0) {
        } else if (val <= 0) {
            ret = -EINVAL;
        } else if (!in_session(t, val)) {
            ret = -EPERM;
        } else {
            t->pgrp = val;
        }
        break;
    case TIOCGWINSZ:
        ws = t->ws;
        break;
    case TIOCSWINSZ:
        if (ws.ws_row != t->ws.ws_row || ws.ws_col != t->ws.ws_col) {
            signal_fg(t, SIGWINCH);
        }
        t->ws = ws;
        break;
    case FIONREAD:
        val = (int)t->in_len;
        break;
    case TIOCSCTTY:
        if (p->sid != p->pid || p->ctty) {
            ret = p->ctty == t ? 0 : -EPERM;
        } else if (t->session && t->session != p->sid) {
            ret = -EPERM;
        } else {
            acquire(t, p);
        }
        break;
    case TIOCNOTTY:
        if (p->ctty != t) {
            ret = -ENOTTY;
        } else if (p->sid == p->pid) {
            disassociate(t);
        } else {
            p->ctty = nullptr;
        }
        break;
    default:
        ret = -ENOTTY;
    }
    spin_unlock_irqrestore(&sched_lock, flags);
    if (ret) {
        return ret;
    }

    switch (req) {
    case TCGETS:
        return copy_to_user(arg, &nt, sizeof(nt));
    case TIOCGWINSZ:
        return copy_to_user(arg, &ws, sizeof(ws));
    case TIOCGPGRP:
    case TIOCGSID:
    case FIONREAD:
        return copy_to_user(arg, &val, sizeof(val));
    }
    return 0;
}
