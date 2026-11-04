#include <unistd.h>

#include <syscall.h>

uid_t getuid() { return (uid_t)__syscall0(SYS_getuid); }
uid_t geteuid() { return (uid_t)__syscall0(SYS_geteuid); }
gid_t getgid() { return (gid_t)__syscall0(SYS_getgid); }
gid_t getegid() { return (gid_t)__syscall0(SYS_getegid); }

int setuid(uid_t uid) {
    return (int)__syscall_ret(__syscall1(SYS_setuid, (int)uid));
}

int setgid(gid_t gid) {
    return (int)__syscall_ret(__syscall1(SYS_setgid, (int)gid));
}

int seteuid(uid_t uid) { return setresuid((uid_t)-1, uid, (uid_t)-1); }
int setegid(gid_t gid) { return setresgid((gid_t)-1, gid, (gid_t)-1); }

int setreuid(uid_t ruid, uid_t euid) {
    return (int)__syscall_ret(__syscall2(SYS_setreuid, (int)ruid, (int)euid));
}

int setregid(gid_t rgid, gid_t egid) {
    return (int)__syscall_ret(__syscall2(SYS_setregid, (int)rgid, (int)egid));
}

int setresuid(uid_t ruid, uid_t euid, uid_t suid) {
    return (int)__syscall_ret(
        __syscall3(SYS_setresuid, (int)ruid, (int)euid, (int)suid));
}

int setresgid(gid_t rgid, gid_t egid, gid_t sgid) {
    return (int)__syscall_ret(
        __syscall3(SYS_setresgid, (int)rgid, (int)egid, (int)sgid));
}

int getgroups(int n, gid_t list[]) {
    return (int)__syscall_ret(__syscall2(SYS_getgroups, n, (long)list));
}

int setgroups(size_t n, const gid_t list[]) {
    return (int)__syscall_ret(__syscall2(SYS_setgroups, n, (long)list));
}
