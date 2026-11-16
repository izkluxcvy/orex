#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_locale_t
#define __NEED_nl_item
#include <bits/alltypes.h>

#define CODESET     0
#define D_T_FMT     1
#define D_FMT       2
#define T_FMT       3
#define T_FMT_AMPM  4
#define AM_STR      5
#define PM_STR      6
#define DAY_1       7
#define DAY_2       8
#define DAY_3       9
#define DAY_4       10
#define DAY_5       11
#define DAY_6       12
#define DAY_7       13
#define ABDAY_1     14
#define ABDAY_2     15
#define ABDAY_3     16
#define ABDAY_4     17
#define ABDAY_5     18
#define ABDAY_6     19
#define ABDAY_7     20
#define MON_1       21
#define MON_2       22
#define MON_3       23
#define MON_4       24
#define MON_5       25
#define MON_6       26
#define MON_7       27
#define MON_8       28
#define MON_9       29
#define MON_10      30
#define MON_11      31
#define MON_12      32
#define ABMON_1     33
#define ABMON_2     34
#define ABMON_3     35
#define ABMON_4     36
#define ABMON_5     37
#define ABMON_6     38
#define ABMON_7     39
#define ABMON_8     40
#define ABMON_9     41
#define ABMON_10    42
#define ABMON_11    43
#define ABMON_12    44
#define ALTMON_1    45
#define ALTMON_2    46
#define ALTMON_3    47
#define ALTMON_4    48
#define ALTMON_5    49
#define ALTMON_6    50
#define ALTMON_7    51
#define ALTMON_8    52
#define ALTMON_9    53
#define ALTMON_10   54
#define ALTMON_11   55
#define ALTMON_12   56
#define ABALTMON_1  57
#define ABALTMON_2  58
#define ABALTMON_3  59
#define ABALTMON_4  60
#define ABALTMON_5  61
#define ABALTMON_6  62
#define ABALTMON_7  63
#define ABALTMON_8  64
#define ABALTMON_9  65
#define ABALTMON_10 66
#define ABALTMON_11 67
#define ABALTMON_12 68
#define ERA         69
#define ERA_D_FMT   70
#define ALT_DIGITS  71
#define ERA_D_T_FMT 72
#define ERA_T_FMT   73
#define RADIXCHAR   74
#define THOUSEP     75
#define YESEXPR     76
#define NOEXPR      77
#define CRNCYSTR    78

char *nl_langinfo(nl_item item);
char *nl_langinfo_l(nl_item item, locale_t loc);

#ifdef __cplusplus
}
#endif
