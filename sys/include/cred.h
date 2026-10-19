#pragma once

#include <stdint.h>

typedef uint32_t uid_t;
typedef uint32_t gid_t;

#define NGROUPS_MAX 16

struct cred {
    uid_t ruid, euid, suid;
    gid_t rgid, egid, sgid;
    int   ngroups;
    gid_t groups[NGROUPS_MAX];
};

int cred_in_group(const struct cred *c, gid_t gid, int real);

long sys_getuid();
long sys_geteuid();
long sys_getgid();
long sys_getegid();
long sys_setuid(long uid);
long sys_setgid(long gid);
long sys_setreuid(long ruid, long euid);
long sys_setregid(long rgid, long egid);
long sys_setresuid(long ruid, long euid, long suid);
long sys_setresgid(long rgid, long egid, long sgid);
long sys_getresuid(uid_t *r, uid_t *e, uid_t *s);
long sys_getresgid(gid_t *r, gid_t *e, gid_t *s);
long sys_getgroups(long size, gid_t *list);
long sys_setgroups(long size, const gid_t *list);
