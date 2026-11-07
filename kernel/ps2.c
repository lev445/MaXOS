#include "ps2.h"
#include "io.h"
#include "idt.h"
#include "pic.h"
#include "console.h"
#include "font.h"

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

static int ps2_wait_write(void)
{
    for (int i = 0; i < 100000; i++)
        if (!(inb(PS2_STATUS) & 0x02))
            return 1;
    return 0;
}
static int ps2_wait_read(void)
{
    for (int i = 0; i < 100000; i++)
        if (inb(PS2_STATUS) & 0x01)
            return 1;
    return 0;
}
static void ps2_cmd(uint8_t c)   { ps2_wait_write(); outb(PS2_CMD, c); }
static void ps2_data(uint8_t c)  { ps2_wait_write(); outb(PS2_DATA, c); }
static uint8_t ps2_resp(void)    { ps2_wait_read();  return inb(PS2_DATA); }
static void mouse_cmd(uint8_t c) { ps2_cmd(0xD4); ps2_data(c); }

#define KBUF 256
static uint8_t kbuf[KBUF];
static volatile unsigned khead, ktail;

static void kbd_push(uint8_t c)
{
    unsigned nx = (khead + 1) & (KBUF - 1);
    if (nx != ktail) {
        kbuf[khead] = c;
        khead = nx;
    }
}

int kbd_avail(void) { return khead != ktail; }

int kbd_getchar(void)
{
    while (khead == ktail) {
        sti();
        hlt();
    }
    uint8_t c = kbuf[ktail];
    ktail = (ktail + 1) & (KBUF - 1);
    return c;
}

static const char map_norm[0x54] = {
    [0x01] = 27,
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = '-', [0x0D] = '=', [0x0E] = 8,   [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '[', [0x1B] = ']', [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x27] = ';',
    [0x28] = '\'', [0x29] = '`', [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x37] = '*', [0x39] = ' ',
};
static const char map_shift[0x54] = {
    [0x01] = 27,
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
    [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',
    [0x0C] = '_', [0x0D] = '+', [0x0E] = 8,   [0x0F] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
    [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
    [0x1A] = '{', [0x1B] = '}', [0x1C] = '\n',
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G',
    [0x23] = 'H', [0x24] = 'J', [0x25] = 'K', [0x26] = 'L', [0x27] = ':',
    [0x28] = '"', [0x29] = '~', [0x2B] = '|',
    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V', [0x30] = 'B',
    [0x31] = 'N', [0x32] = 'M', [0x33] = '<', [0x34] = '>', [0x35] = '?',
    [0x37] = '*', [0x39] = ' ',
};

static int shift, ctrl, alt, caps, e0;

static void kbd_irq(void)
{
    uint8_t sc = inb(PS2_DATA);

    if (sc == 0xE0) {
        e0 = 1;
        return;
    }
    if (sc & 0x80) {                       /* break */
        uint8_t make = sc & 0x7F;
        if (e0) {
            e0 = 0;
            if (make == 0x1D) ctrl = 0;
            if (make == 0x38) alt = 0;
            return;
        }
        if (make == 0x2A || make == 0x36) shift = 0;
        else if (make == 0x1D) ctrl = 0;
        else if (make == 0x38) alt = 0;
        return;
    }
    /* make */
    if (e0) {
        e0 = 0;
        switch (sc) {
        case 0x48: kbd_push(KEY_UP);    return;
        case 0x50: kbd_push(KEY_DOWN);  return;
        case 0x4B: kbd_push(KEY_LEFT);  return;
        case 0x4D: kbd_push(KEY_RIGHT); return;
        case 0x47: kbd_push(KEY_HOME);  return;
        case 0x4F: kbd_push(KEY_END);   return;
        case 0x53: kbd_push(KEY_DEL);   return;
        case 0x49: kbd_push(KEY_PGUP);  return;
        case 0x51: kbd_push(KEY_PGDN);  return;
        case 0x52: kbd_push(KEY_INS);   return;
        case 0x1D: ctrl = 1;            return;
        case 0x38: alt = 1;             return;
        default:   return;
        }
    }
    switch (sc) {
    case 0x2A: case 0x36: shift = 1; return;
    case 0x1D: ctrl = 1; return;
    case 0x38: alt = 1;  return;
    case 0x3A: caps = !caps; return;
    case 0x3B: kbd_push(KEY_F1); return;
    case 0x3C: kbd_push(KEY_F2); return;
    case 0x3D: kbd_push(KEY_F3); return;
    }
    if (sc >= 0x54)
        return;
    char base = map_norm[sc];
    if (!base)
        return;
    char c;
    if (base >= 'a' && base <= 'z')
        c = (caps ^ shift) ? (char)(base - 32) : base;
    else
        c = shift ? map_shift[sc] : base;
    if (ctrl && c >= 'a' && c <= 'z') { kbd_push((uint8_t)(c - 'a' + 1)); return; }
    if (ctrl && c >= 'A' && c <= 'Z') { kbd_push((uint8_t)(c - 'A' + 1)); return; }
    kbd_push((uint8_t)c);
}

static int mouse_on;
static uint8_t mcycle;
static uint8_t mpkt[3];
static int mx, my, mbtn;

static void mouse_clamp(void)
{
    int w = console_cols() * FONT_W - 1;
    int h = console_rows() * FONT_H - 1;
    if (mx < 0) mx = 0;
    if (my < 0) my = 0;
    if (mx > w) mx = w;
    if (my > h) my = h;
}

static void mouse_irq(void)
{
    uint8_t d = inb(PS2_DATA);
    if (!mouse_on)
        return;
    switch (mcycle) {
    case 0:
        if (d & 0x08) { mpkt[0] = d; mcycle = 1; }
        break;
    case 1:
        mpkt[1] = d; mcycle = 2;
        break;
    default:
        mpkt[2] = d; mcycle = 0;
        {
            int dx = mpkt[1], dy = mpkt[2];
            if (mpkt[0] & 0x10) dx -= 256;
            if (mpkt[0] & 0x20) dy -= 256;
            mbtn = mpkt[0] & 0x07;
            mx += dx;
            my -= dy;
            mouse_clamp();
        }
        break;
    }
}

void mouse_get(int *x, int *y, int *b)
{
    if (x) *x = mx;
    if (y) *y = my;
    if (b) *b = mbtn;
}
int mouse_enabled(void) { return mouse_on; }

void mouse_enable(int on)
{
    if (on && !mouse_on) {
        pic_set_mask(12, 1);
        mouse_cmd(0xF6); ps2_resp();      /* defaults */
        mouse_cmd(0xF4); ps2_resp();      /* enable reporting */
        mcycle = 0;
        mouse_on = 1;
    } else if (!on && mouse_on) {
        mouse_cmd(0xF5); ps2_resp();      /* disable reporting */
        pic_set_mask(12, 0);
        mouse_on = 0;
    }
}

/* ---------- инициализация ---------- */
void ps2_init(void)
{
    ps2_cmd(0xAD);            /* disable keyboard  */
    ps2_cmd(0xA7);            /* disable mouse     */
    while (inb(PS2_STATUS) & 0x01)
        inb(PS2_DATA);        /* flush */

    ps2_cmd(0x20);
    uint8_t cfg = ps2_resp();
    cfg |= 0x01;              /* IRQ1  (kbd)  on  */
    cfg |= 0x02;              /* IRQ12 (mouse) on */
    cfg &= ~0x30;             /* устройства включены */
    cfg |= 0x40;              /* трансляция set2 -> set1 (как на реальном PC) */
    ps2_cmd(0x60);
    ps2_data(cfg);

    ps2_cmd(0xAE);            /* enable keyboard */
    ps2_cmd(0xA8);            /* enable mouse    */

    irq_register(1, kbd_irq);
    irq_register(12, mouse_irq);
    pic_set_mask(1, 1);

    mx = console_cols() * FONT_W / 2;
    my = console_rows() * FONT_H / 2;
    mouse_enable(1);
}
