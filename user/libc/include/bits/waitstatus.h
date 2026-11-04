#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define WNOHANG    1
#define WUNTRACED  2
#define WSTOPPED   WUNTRACED
#define WEXITED    4
#define WCONTINUED 8
#define WNOWAIT    0x01000000

#define WEXITSTATUS(s)  (((s) & 0xff00) >> 8)
#define WTERMSIG(s)     ((s) & 0x7f)
#define WSTOPSIG(s)     WEXITSTATUS(s)
#define WIFEXITED(s)    (!WTERMSIG(s))
#define WIFSIGNALED(s)  (((s) & 0xffff) - 1U < 0xffu)
#define WIFSTOPPED(s)   (((s) & 0xff) == 0x7f)
#define WIFCONTINUED(s) ((s) == 0xffff)
#define WCOREDUMP(s)    ((s) & 0x80)

#ifdef __cplusplus
}
#endif
