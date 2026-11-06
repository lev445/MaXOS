#include "console.h"
#include "font.h"
#include "string.h"

#define CONSOLE_MAX_CELLS (256 * 128)

static volatile uint8_t *fb;
static uint32_t pitch, width, height, bpp;
static uint32_t cols, rows;
static uint8_t rpos, gpos, bpos, rmask, gmask, bmask;
static int ready;

static uint16_t shadow[CONSOLE_MAX_CELLS];
static uint8_t cur_fg = COL_LGRAY, cur_bg = COL_BLACK;
static int cur_x, cur_y;
static int cur_vis = 1;
static int drawn_x = -1, drawn_y = -1;

/* стандартная VGA-палитра (R,G,B) */
static const uint8_t pal[16][3] = {
    {0x00,0x00,0x00},{0x00,0x00,0xAA},{0x00,0xAA,0x00},{0x00,0xAA,0xAA},
    {0xAA,0x00,0x00},{0xAA,0x00,0xAA},{0xAA,0x55,0x00},{0xAA,0xAA,0xAA},
    {0x55,0x55,0x55},{0x55,0x55,0xFF},{0x55,0xFF,0x55},{0x55,0xFF,0xFF},
    {0xFF,0x55,0x55},{0xFF,0x55,0xFF},{0xFF,0xFF,0x55},{0xFF,0xFF,0xFF},
};

static uint32_t make_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t p = 0;
    if (rmask) p |= ((uint32_t)(r >> (8 - rmask)) << rpos);
    if (gmask) p |= ((uint32_t)(g >> (8 - gmask)) << gpos);
    if (bmask) p |= ((uint32_t)(b >> (8 - bmask)) << bpos);
    return p;
}

static uint32_t color_pixel(uint8_t idx)
{
    idx &= 0x0F;
    return make_pixel(pal[idx][0], pal[idx][1], pal[idx][2]);
}

static inline uint16_t pack(uint8_t ch, uint8_t fg, uint8_t bg)
{
    return (uint16_t)ch | ((uint16_t)(fg & 0xF) << 8) | ((uint16_t)(bg & 0xF) << 12);
}
static inline uint8_t cell_ch(uint16_t c) { return c & 0xFF; }
static inline uint8_t cell_fg(uint16_t c) { return (c >> 8) & 0xF; }
static inline uint8_t cell_bg(uint16_t c) { return (c >> 12) & 0xF; }

static void draw_cell_at(int x, int y)
{
    if (!ready || x < 0 || y < 0 || x >= (int)cols || y >= (int)rows)
        return;
    uint16_t c = shadow[y * cols + x];
    uint8_t fg = cell_fg(c), bg = cell_bg(c);
    if (cur_vis && x == cur_x && y == cur_y) {
        uint8_t t = fg; fg = bg; bg = t;
    }
    uint32_t fgp = color_pixel(fg), bgp = color_pixel(bg);
    uint8_t ch = cell_ch(c);
    volatile uint8_t *row = fb + (uint64_t)y * FONT_H * pitch + (uint64_t)x * FONT_W * 4;
    for (int r = 0; r < FONT_H; r++) {
        uint8_t bits = font8x16[ch][r];
        volatile uint8_t *p = row;
        for (int b = 0; b < FONT_W; b++) {
            uint32_t px = (bits >> (7 - b)) & 1 ? fgp : bgp;
            *(uint32_t *)(uintptr_t)p = px;
            p += 4;
        }
        row += pitch;
    }
}

static void move_cursor(int x, int y)
{
    int ox = drawn_x, oy = drawn_y;
    cur_x = x; cur_y = y;
    if (ox >= 0)
        draw_cell_at(ox, oy);
    draw_cell_at(cur_x, cur_y);
    drawn_x = cur_x; drawn_y = cur_y;
}

void console_init(const boot_info_t *bi)
{
    if (!bi || bi->magic != 0x31454256 || !bi->ok || bi->bpp != 32 || !bi->fb)
        return;
    fb = (volatile uint8_t *)(uintptr_t)bi->fb;
    pitch = bi->pitch;
    width = bi->width;
    height = bi->height;
    bpp = bi->bpp;
    rpos = bi->rpos; gpos = bi->gpos; bpos = bi->bpos;
    rmask = bi->rmask; gmask = bi->gmask; bmask = bi->bmask;
    cols = width / FONT_W;
    rows = height / FONT_H;
    if (cols * rows > CONSOLE_MAX_CELLS) {
        cols = CONSOLE_MAX_CELLS / rows;
    }
    ready = 1;
    console_clear();
}

int console_ready(void) { return ready; }
int console_cols(void) { return (int)cols; }
int console_rows(void) { return (int)rows; }

void console_refresh(void)
{
    if (!ready)
        return;
    for (uint32_t y = 0; y < rows; y++)
        for (uint32_t x = 0; x < cols; x++)
            draw_cell_at(x, y);
    drawn_x = cur_x; drawn_y = cur_y;
}

void console_clear(void)
{
    if (!ready)
        return;
    uint16_t blank = pack(' ', cur_fg, cur_bg);
    for (uint32_t i = 0; i < cols * rows; i++)
        shadow[i] = blank;
    console_refresh();
    move_cursor(0, 0);
}

void console_scroll(int lines)
{
    if (!ready || lines <= 0)
        return;
    if (lines >= (int)rows) {
        console_clear();
        return;
    }
    memmove(&shadow[0], &shadow[(size_t)lines * cols],
            (size_t)(rows - lines) * cols * sizeof(uint16_t));
    uint16_t blank = pack(' ', cur_fg, cur_bg);
    for (uint32_t i = (rows - lines) * cols; i < cols * rows; i++)
        shadow[i] = blank;
    console_refresh();
}

void console_set_color(uint8_t fg, uint8_t bg)
{
    cur_fg = fg & 0xF;
    cur_bg = bg & 0xF;
}
void console_get_color(uint8_t *fg, uint8_t *bg)
{
    if (fg) *fg = cur_fg;
    if (bg) *bg = cur_bg;
}

void console_set_cursor(int x, int y)
{
    if (!ready)
        return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= (int)cols) x = cols - 1;
    if (y >= (int)rows) y = rows - 1;
    move_cursor(x, y);
}
void console_get_cursor(int *x, int *y)
{
    if (x) *x = cur_x;
    if (y) *y = cur_y;
}

void console_cursor_visible(int vis)
{
    vis = !!vis;
    if (vis == cur_vis)
        return;
    cur_vis = vis;
    draw_cell_at(cur_x, cur_y);
}

void console_putc(char c)
{
    if (!ready)
        return;
    if (c == '\n') {
        int ny = cur_y + 1;
        if (ny >= (int)rows) {
            console_scroll(1);
            ny = rows - 1;
        }
        move_cursor(0, ny);
        return;
    }
    if (c == '\r') {
        move_cursor(0, cur_y);
        return;
    }
    if (c == '\b') {
        if (cur_x > 0) {
            shadow[cur_y * cols + (cur_x - 1)] = pack(' ', cur_fg, cur_bg);
            move_cursor(cur_x - 1, cur_y);
        }
        return;
    }
    if (c == '\t') {
        int nx = (cur_x + 8) & ~7;
        if (nx >= (int)cols)
            nx = cols - 1;
        move_cursor(nx, cur_y);
        return;
    }
    shadow[cur_y * cols + cur_x] = pack((uint8_t)c, cur_fg, cur_bg);
    int nx = cur_x + 1, ny = cur_y;
    if (nx >= (int)cols) {
        nx = 0;
        ny++;
        if (ny >= (int)rows) {
            console_scroll(1);
            ny = rows - 1;
        }
    }
    move_cursor(nx, ny);
}

void console_write(const char *s, int n)
{
    for (int i = 0; i < n; i++)
        console_putc(s[i]);
}

void console_puts(const char *s)
{
    while (*s)
        console_putc(*s++);
}
