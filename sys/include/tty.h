#pragma once

#include <stddef.h>
#include <stdint.h>

#include <waitq.h>

struct file;
struct proc;

#define NCCS 19
struct termios {
    uint32_t c_iflag, c_oflag, c_cflag, c_lflag;
    uint8_t  c_line;
    uint8_t  c_cc[NCCS];
};

struct winsize {
    uint16_t ws_row, ws_col, ws_xpixel, ws_ypixel;
};

#define TTY_BUF  4096
#define TTY_LINE 256
#define TTY_EOLS 64

struct tty {
    struct termios t;
    struct winsize ws;
    int            session;
    int            pgrp;

    char   line[TTY_LINE];
    size_t line_len;
    char   in[TTY_BUF];
    size_t in_head, in_len;
    size_t eol[TTY_EOLS];
    size_t eol_head, eol_count;
    int    lnext;
    int    out_stopped;
    int    hungup;

    struct waitq readers, writers;

    size_t (*output)(struct tty *tty, const char *p, size_t n);
    void *priv;
    int   index;
};

struct tty *tty_console(void);
struct tty *tty_fb(void);
void        tty_console_select(const char *name);
struct tty *tty_serial(void);
void        tty_init(struct tty *tty,
                     size_t (*output)(struct tty *, const char *, size_t));

void tty_input(struct tty *tty, char c);
void tty_input_locked(struct tty *tty, char c);
long tty_read(struct tty *tty, void *buf, size_t n, int nonblock);
long tty_write(struct tty *tty, const void *buf, size_t n, int nonblock);
long tty_ioctl(struct tty *tty, struct file *f, unsigned long req, void *arg);
void tty_hangup_locked(struct tty *tty);

void tty_opened(struct tty *tty, int flags);

void tty_session_ended_locked(struct proc *p);

#define TCGETS     0x5401
#define TCSETS     0x5402
#define TCSETSW    0x5403
#define TCSETSF    0x5404
#define TCSBRK     0x5409
#define TCXONC     0x540a
#define TCFLSH     0x540b
#define TIOCSCTTY  0x540e
#define TIOCGPGRP  0x540f
#define TIOCSPGRP  0x5410
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define FIONREAD   0x541b
#define TIOCNOTTY  0x5422
#define TIOCGSID   0x5429
#define TIOCGPTN   0x80045430
#define TIOCSPTLCK 0x40045431

// c_iflag
#define IGNBRK 0000001
#define BRKINT 0000002
#define ISTRIP 0000040
#define INLCR  0000100
#define IGNCR  0000200
#define ICRNL  0000400
#define IXON   0002000
#define IXANY  0004000
// c_oflag
#define OPOST  0000001
#define ONLCR  0000004
#define OCRNL  0000010
#define ONOCR  0000020
#define ONLRET 0000040
// c_cflag
#define B38400 0000017
#define CS8    0000060
#define CREAD  0000200
#define HUPCL  0002000
// c_lflag
#define ISIG    0000001
#define ICANON  0000002
#define ECHO    0000010
#define ECHOE   0000020
#define ECHOK   0000040
#define ECHONL  0000100
#define NOFLSH  0000200
#define TOSTOP  0000400
#define ECHOCTL 0001000
#define ECHOKE  0004000
#define IEXTEN  0100000
// c_cc
#define VINTR    0
#define VQUIT    1
#define VERASE   2
#define VKILL    3
#define VEOF     4
#define VTIME    5
#define VMIN     6
#define VSTART   8
#define VSTOP    9
#define VSUSP    10
#define VEOL     11
#define VREPRINT 12
#define VWERASE  14
#define VLNEXT   15
#define VEOL2    16
