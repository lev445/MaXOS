#include "rtc.h"
#include "io.h"

static uint8_t cmos_read(uint8_t reg)
{
    outb(0x70, 0x80 | reg);   /* NMI disable + выбор регистра */
    return inb(0x71);
}

static int bcd(int v) { return (v & 0xF) + ((v >> 4) & 0xF) * 10; }

void rtc_read(rtc_time_t *t)
{
    /* ждём окончания обновления */
    while (cmos_read(0x0A) & 0x80)
        ;
    int sec  = cmos_read(0x00);
    int min  = cmos_read(0x02);
    int hour = cmos_read(0x04);
    int day  = cmos_read(0x07);
    int mon  = cmos_read(0x08);
    int year = cmos_read(0x09);
    uint8_t b = cmos_read(0x0B);
    if (!(b & 0x04)) {
        sec = bcd(sec); min = bcd(min); day = bcd(day);
        mon = bcd(mon); year = bcd(year);
        int hh = hour & 0x7F;
        hh = bcd(hh);
        if (!(b & 0x02) && (hour & 0x80))   /* 12h + PM */
            hh = (hh % 12) + 12;
        hour = hh;
    }
    t->sec = sec; t->min = min; t->hour = hour;
    t->day = day; t->mon = mon;
    t->year = year + (year < 70 ? 2000 : 1900);
}
