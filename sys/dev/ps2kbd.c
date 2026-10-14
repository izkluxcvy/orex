#include <stdint.h>

#include <printf.h>

#include "io.h"
#include "ioapic.h"
#include "irq.h"
#include "trap.h"

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

#define STATUS_OUT_FULL 0x01
#define STATUS_IN_FULL  0x02

#define CMD_READ_CONFIG   0x20
#define CMD_WRITE_CONFIG  0x60
#define CMD_DISABLE_PORT2 0xa7
#define CMD_DISABLE_PORT1 0xad
#define CMD_ENABLE_PORT1  0xae

#define CONFIG_PORT1_IRQ   0x01
#define CONFIG_PORT1_CLOCK 0x10
#define CONFIG_TRANSLATE   0x40

#define KBD_ENABLE_SCAN 0xf4

#define KBD_IRQ 1

#define SC_LSHIFT 0x2a
#define SC_RSHIFT 0x36
#define SC_CTRL   0x1d
#define SC_CAPS   0x3a
#define SC_EXT    0xe0
#define SC_BREAK  0x80

static const char keymap[128] = {
    0,   27,  '1',  '2',  '3',  '4', '5', '6',  '7', '8', '9', '0',
    '-', '=', 0x7f, '\t', 'q',  'w', 'e', 'r',  't', 'y', 'u', 'i',
    'o', 'p', '[',  ']',  '\r', 0,   'a', 's',  'd', 'f', 'g', 'h',
    'j', 'k', 'l',  ';',  '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm',  ',',  '.',  '/', 0,   '*',  0,   ' ',
};
static const char keymap_shift[128] = {
    0,   27,  '!',  '@',  '#',  '$', '%', '^', '&', '*', '(', ')',
    '_', '+', 0x7f, '\t', 'Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{',  '}',  '\r', 0,   'A', 'S', 'D', 'F', 'G', 'H',
    'J', 'K', 'L',  ':',  '"',  '~', 0,   '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M',  '<',  '>',  '?', 0,   '*', 0,   ' ',
};

static int shift, ctrl, caps, extended;

static void input(char c) {
    char s[2] = {c >= ' ' && c < 0x7f ? c : '?', '\0'};
    printf("ps2kbd: %x '%s'\n", (unsigned)(unsigned char)c, s);
}

static const char *special(int ext, uint8_t code) {
    if (ext) {
        switch (code) {
        case 0x48:
            return "\033[A]";
        case 0x50:
            return "\033[B]";
        case 0x4d:
            return "\033[C]";
        case 0x4b:
            return "\033[D]";
        case 0x47:
            return "\033[1~]";
        case 0x52:
            return "\033[2~]";
        case 0x53:
            return "\033[3~]";
        case 0x4f:
            return "\033[4~]";
        case 0x49:
            return "\033[5~]";
        case 0x51:
            return "\033[6~]";
        case 0x1c:
            return "\r";
        case 0x35:
            return "/";
        }
        return nullptr;
    }
    switch (code) {
    case 0x3b:
        return "\033OP";
    case 0x3c:
        return "\033OQ";
    case 0x3d:
        return "\033OR";
    case 0x3e:
        return "\033OS";
    case 0x3f:
        return "\033[15~";
    case 0x40:
        return "\033[17~";
    case 0x41:
        return "\033[18~";
    case 0x42:
        return "\033[19~";
    case 0x43:
        return "\033[20~";
    case 0x44:
        return "\033[21~";
    case 0x57:
        return "\033[23~";
    case 0x58:
        return "\033[24~";
    }
    return nullptr;
}

static void wait_write() {
    for (int i = 0; i < 100000 && (inb(PS2_STATUS) & STATUS_IN_FULL); i++) {
    }
}

static int wait_read() {
    for (int i = 0; i < 100000; i++) {
        if (inb(PS2_STATUS) & STATUS_OUT_FULL) {
            return 1;
        }
    }
    return 0;
}

static void command(uint8_t cmd) {
    wait_write();
    outb(PS2_CMD, cmd);
}

static void key(uint8_t sc) {
    if (sc == SC_EXT) {
        extended = 1;
        return;
    }
    int ext  = extended;
    extended = 0;

    int     down = !(sc & SC_BREAK);
    uint8_t code = sc & ~SC_BREAK;

    if (code == SC_LSHIFT || code == SC_RSHIFT) {
        shift = down;
        return;
    }
    if (code == SC_CTRL) {
        ctrl = down;
        return;
    }
    if (!down) {
        return;
    }
    const char *seq = special(ext, code);
    if (seq) {
        for (; *seq; seq++) {
            input(*seq);
        }
        return;
    }
    if (ext) {
        return;
    }
    if (code == SC_CAPS) {
        caps = !caps;
        return;
    }

    char c = (shift ? keymap_shift : keymap)[code];
    if (caps && c >= 'a' && c <= 'z') {
        c -= 'a' - 'A';
    } else if (caps && c >= 'A' && c <= 'Z') {
        c += 'a' - 'A';
    }
    if (ctrl && c) {
        c &= 0x1f;
    }
    if (c) {
        input(c);
    }
}

static void ps2kbd_irq(struct trapframe *tf) {
    (void)tf;
    while (inb(PS2_STATUS) & STATUS_OUT_FULL) {
        key(inb(PS2_DATA));
    }
}

void ps2kbd_init() {
    command(CMD_DISABLE_PORT1);
    command(CMD_DISABLE_PORT2);
    while (inb(PS2_STATUS) & STATUS_OUT_FULL) {
        inb(PS2_DATA);
    }

    command(CMD_READ_CONFIG);
    uint8_t config = wait_read() ? inb(PS2_DATA) : 0;
    config |= CONFIG_PORT1_IRQ | CONFIG_TRANSLATE;
    config &= ~CONFIG_PORT1_CLOCK;
    command(CMD_WRITE_CONFIG);
    wait_write();
    outb(PS2_DATA, config);

    command(CMD_ENABLE_PORT1);
    wait_write();
    outb(PS2_DATA, KBD_ENABLE_SCAN);
    if (wait_read()) {
        inb(PS2_DATA);
    }

    irq_register(IRQ_VECTOR_ISA + KBD_IRQ, ps2kbd_irq);
    ioapic_route_isa(KBD_IRQ, IRQ_VECTOR_ISA + KBD_IRQ);
    printf("ps2kbd: initialized\n");
}
