#include <stddef.h>
#include <stdint.h>

#include <boot_info.h>
#include <console.h>
#include <memmove.h>
#include <printf.h>

#include "font8x16.h"
#include "pmap.h"

enum { PIXEL_FORMAT_RGB = 0, PIXEL_FORMAT_BGR = 1 };

#define MAX_COLS 256
#define MAX_ROWS 128
#define DEF      16 // default color

struct cell {
    uint8_t ch, attr, fg, bg;
};

static uint32_t   *fb;
static size_t      pitch;
static int         cols, rows;
static struct cell screen[MAX_ROWS * MAX_COLS];
static uint32_t    palette[18]; // 16 + fg and bg
static uint8_t     tab_stop[MAX_COLS];

static struct {
    int     col, row;
    int     wrap;
    uint8_t attr, fg, bg;
    int     top, bottom;
    int     autowrap, cursor_on;
} t;

static uint32_t pack(size_t format, uint8_t r, uint8_t g, uint8_t b) {
    if (format == PIXEL_FORMAT_BGR) {
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
    return ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
}

static struct cell *at(int row, int col) { return &screen[row + cols * col]; }

static struct cell blank() {
    return (struct cell){.ch = ' ', .attr = 0, .fg = t.fg, .bg = t.bg};
}

static void draw(int row, int col) {
    const struct cell *c  = at(row, col);
    int                fi = c->fg == DEF ? 16 : c->fg;
    int                bi = c->bg == DEF ? 17 : c->bg;
    uint32_t           f = palette[fi], b = palette[bi];
    if (row == t.row && col == t.col && t.cursor_on) {
        uint32_t x = f;
        f          = b;
        b          = x;
    }

    uint8_t ch =
        c->ch >= FONT8X16_FIRST && c->ch <= FONT8X16_LAST ? c->ch : '?';
    const uint8_t *glyph = font8x16[ch - FONT8X16_FIRST];
    uint32_t      *dest =
        fb + (size_t)row * FONT8X16_H * pitch + (size_t)col * FONT8X16_W;
    for (int y = 0; y < FONT8X16_H; y++) {
        uint8_t bits = glyph[y];
        for (int x = 0; x < FONT8X16_W; x++) {
            dest[x] = (bits & (0x80 >> x)) ? f : b;
        }
        dest += pitch;
    }
}

static void draw_rows(int from, int to) {
    for (int r = from; r <= to; r++) {
        for (int c = 0; c < cols; c++) {
            draw(r, c);
        }
    }
}

static void move_to(int row, int col) {
    int orow = t.row, ocol = t.col;
    t.row  = row < 0 ? 0 : row >= rows ? rows - 1 : row;
    t.col  = col < 0 ? 0 : col >= cols ? cols - 1 : col;
    t.wrap = 0;
    if (orow < rows && ocol < cols) {
        draw(orow, ocol);
    }
    draw(t.row, t.col);
}

static void clear_cells(int row, int from, int to) {
    for (int c = from; c <= to; c++) {
        *at(row, c) = blank();
        draw(row, c);
    }
}

static void scroll_region(int top, int bottom, int n) {
    int height = bottom - top + 1;
    if (n > height) {
        n = height;
    } else if (n < -height) {
        n = -height;
    }
    if (!n) {
        return;
    }

    int    k    = n > 0 ? n : -n;
    size_t line = pitch * FONT8X16_H;
    int    on   = t.cursor_on;
    t.cursor_on = 0;
    draw(t.row, t.col);
    t.cursor_on = on;
    if (n > 0) {
        memmove(at(top, 0), at(top + k, 0),
                (size_t)(height - k) * cols * sizeof(struct cell));
        memmove(fb + top * line, fb + (top + k) * line,
                (height - k) * line * sizeof(*fb));
        for (int r = bottom - k + 1; r <= bottom; r++) {
            clear_cells(r, 0, cols - 1);
        }
    } else {
        memmove(at(top + k, 0), at(top, 0),
                (size_t)(height - k) * cols * sizeof(struct cell));
        memmove(fb + (top + k) * line, fb + top * line,
                (height - k) * line * sizeof(*fb));
        for (int r = top; r < top + k; r++) {
            clear_cells(r, 0, cols - 1);
        }
    }
    draw(t.row, t.col);
}

static void index_down() {
    if (t.row == t.bottom) {
        scroll_region(t.top, t.bottom, 1);
    } else if (t.row < rows - 1) {
        move_to(t.row + 1, t.col);
    }
}

static void put_glyph(uint8_t ch) {
    if (t.wrap && t.autowrap) {
        move_to(t.row, 0);
        index_down();
    }
    *at(t.row, t.col) = (struct cell){ch, t.attr, t.fg, t.bg};
    if (t.col == cols - 1) {
        t.wrap = 1;
        draw(t.row, t.col);
    } else {
        move_to(t.row, t.col + 1);
    }
}

static void reset() {
    for (int c = 0; c < MAX_COLS; c++) {
        tab_stop[c] = c && c % 8 == 0;
    }
    t.attr = 0, t.fg = DEF, t.bg = DEF;
    t.top = 0, t.bottom = rows - 1;
    t.autowrap = 1, t.cursor_on = 1;
}

static void control(char c) {
    switch (c) {
    case '\n':
    case '\v':
    case '\f':
        index_down();
        break;
    case '\r':
        move_to(t.row, 0);
        break;
    case '\b':
        move_to(t.row, t.col - 1);
        break;
    case '\t': {
        int next = t.col + 1;
        while (next < cols - 1 && !tab_stop[next]) {
            next++;
        }
        move_to(t.row, next >= cols ? cols - 1 : next);
        break;
    }
    }
}

void console_putc(char ch) {
    if (!fb) {
        return;
    }
    unsigned char c = (unsigned char)ch;
    if (c < 0x20 || c == 0x7F) {
        control((char)c);
        return;
    }
    put_glyph(c);
}

void console_init(const struct framebuffer *fbinfo) {
    size_t fmt = fbinfo->pixel_format;
    if (!fbinfo->base || fbinfo->width < FONT8X16_W ||
        fbinfo->height < FONT8X16_H) {
        printf("console: no framebuffer\n");
        return;
    }
    if (fmt != PIXEL_FORMAT_RGB && fmt != PIXEL_FORMAT_BGR) {
        printf("console: unsupported pixel format %lu\n", fmt);
        return;
    }

    pitch                           = fbinfo->pixels_per_scanline;
    cols                            = (int)(fbinfo->width / FONT8X16_W);
    rows                            = (int)(fbinfo->height / FONT8X16_H);
    cols                            = cols > MAX_COLS ? MAX_COLS : cols;
    rows                            = rows > MAX_ROWS ? MAX_ROWS : rows;
    static const uint8_t vga[16][3] = {
        {0x00, 0x00, 0x00}, {0xaa, 0x00, 0x00}, {0x00, 0xaa, 0x00},
        {0xaa, 0x55, 0x00}, {0x00, 0x00, 0xaa}, {0xaa, 0x00, 0xaa},
        {0x00, 0xaa, 0xaa}, {0xaa, 0xaa, 0xaa}, {0x55, 0x55, 0x55},
        {0xff, 0x55, 0x55}, {0x55, 0xff, 0x55}, {0xff, 0xff, 0x55},
        {0x55, 0x55, 0xff}, {0xff, 0x55, 0xff}, {0x55, 0xff, 0xff},
        {0xff, 0xff, 0xff},
    };
    for (int i = 0; i < 16; i++) {
        palette[i] = pack(fmt, vga[i][0], vga[i][1], vga[i][2]);
    }
    palette[16] = pack(fmt, 0xC0, 0xC0, 0xC0);
    palette[17] = pack(fmt, 0x00, 0x00, 0x00);
    fb          = phys_to_virt(fbinfo->base);
    reset();
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            *at(r, c) = blank();
        }
    }
    for (size_t i = 0; i < pitch * fbinfo->height; i++) {
        fb[i] = palette[17];
    }
    draw_rows(0, rows - 1);
    printf_add_sink(console_putc);

    printf("console: initialized %lux%lu, %dx%d chars\n", fbinfo->width,
           fbinfo->height, cols, rows);
}
