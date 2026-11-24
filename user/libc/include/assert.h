#ifdef __cplusplus
extern "C" {
#endif

#undef assert
#ifdef NDEBUG
#define assert(e) ((void)0)
#else
#define assert(e)                                                              \
    ((e) ? (void)0 : __assert_fail(#e, __FILE__, __LINE__, __func__))
#endif

#if __STDC_VERSION__ >= 201112L && __STDC_VERSION__ < 202311L &&               \
    !defined(static_assert)
#define static_assert _Static_assert
#endif

__attribute__((__noreturn__)) void
__assert_fail(const char *expr, const char *file, int line, const char *func);

#ifdef __cplusplus
}
#endif
