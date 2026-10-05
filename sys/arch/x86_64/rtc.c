#include <stdint.h>

#include "io.h"

#define CMOS_INDEX 0x70
#define CMOS_DATA  0x71
#define RTC_A      0x0A
#define RTC_B      0x0B
#define RTC_UIP    0x80
#define RTC_BINARY 0x04
#define RTC_24H    0x02

static uint8_t cmos(uint8_t reg) {
    outb(CMOS_INDEX, reg);
    return inb(CMOS_DATA);
}

struct rtc_time {
    uint8_t sec, min, hour, day, month, year;
};

static void rtc_snapshot(struct rtc_time *t) {
    while (cmos(RTC_A) & RTC_UIP) {
    }
    *t = (struct rtc_time){cmos(0x00), cmos(0x02), cmos(0x04),
                           cmos(0x07), cmos(0x08), cmos(0x09)};
}

static int bcd(int v) { return (v & 0x0F) + (v >> 4) * 10; }

static int64_t days_from_epoch(int y, int m, int d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int     yoe = y - (int)era * 400;
    int     doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int     doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

uint64_t rtc_read() {
    struct rtc_time a, b;
    do {
        rtc_snapshot(&a);
        rtc_snapshot(&b);
    } while (a.sec != b.sec || a.min != b.min || a.hour != b.hour ||
             a.day != b.day || a.month != b.month || a.year != b.year);

    uint8_t fmt   = cmos(RTC_B);
    int     pm    = b.hour & 0x80;
    int     sec   = b.sec;
    int     min   = b.min;
    int     hour  = b.hour & 0x7F;
    int     day   = b.day;
    int     month = b.month;
    int     year  = b.year;
    if (!(fmt & RTC_BINARY)) {
        sec = bcd(sec), min = bcd(min), hour = bcd(hour);
        day = bcd(day), month = bcd(month), year = bcd(year);
    }
    if (!(fmt & RTC_24H)) {
        hour = (hour % 12) + (pm ? 12 : 0);
    }
    return (uint64_t)(days_from_epoch(2000 + year, month, day) * 86400 +
                      hour * 3600 + min * 60 + sec);
}
