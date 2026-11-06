#include "pic.h"
#include "io.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT 0x11
#define ICW4_8086 0x01

static uint16_t mask = 0xFFFF;    /* все замаскированы */

static void apply_mask(void)
{
    outb(PIC1_DATA, (uint8_t)(mask & 0xFF));
    outb(PIC2_DATA, (uint8_t)((mask >> 8) & 0xFF));
}

void pic_init(void)
{
    /* каскад 8259: ремап векторов на 0x20..0x2F */
    outb(PIC1_CMD, ICW1_INIT); io_wait();
    outb(PIC2_CMD, ICW1_INIT); io_wait();
    outb(PIC1_DATA, 0x20);     io_wait();   /* IRQ0..7  -> 0x20..0x27 */
    outb(PIC2_DATA, 0x28);     io_wait();   /* IRQ8..15 -> 0x28..0x2F */
    outb(PIC1_DATA, 0x04);     io_wait();   /* IRQ2 = каскад на PIC2 */
    outb(PIC2_DATA, 0x02);     io_wait();
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();
    mask = 0xFFFF;
    apply_mask();
}

void pic_set_mask(uint8_t irq, int enable)
{
    if (irq > 15)
        return;
    if (enable)
        mask &= ~(1u << irq);
    else
        mask |= (1u << irq);
    apply_mask();
    /* если включаем линию каскада - не забыть про IRQ2 */
    if (irq >= 8 && enable)
        pic_set_mask(2, 1);
}

void pic_eoi(uint8_t irq)
{
    if (irq >= 8)
        outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}
