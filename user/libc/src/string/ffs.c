#include <strings.h>

int ffs(int i) { return __builtin_ffs(i); }
int ffsl(long i) { return __builtin_ffsl(i); }
int ffsll(long long i) { return __builtin_ffsll(i); }
