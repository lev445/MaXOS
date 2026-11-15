#ifndef MAXOS_RTC_H
#define MAXOS_RTC_H

typedef struct {
    int sec, min, hour, day, mon, year;
} rtc_time_t;

void rtc_read(rtc_time_t *t);

#endif
