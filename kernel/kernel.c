/* kernel.c -- the actual operating system.
 *
 * Everything here is written from scratch: no Linux, no libc, nothing
 * downloaded at build time except the compiler itself.
 *
 * Draws text by rendering a bitmap font into a linear graphics framebuffer
 * (the address/pitch/size of which GRUB hands us via the Multiboot info
 * struct). This works under both legacy BIOS and pure UEFI, since UEFI
 * doesn't provide the old VGA text-mode memory this kernel used before.
 */

#include <stdint.h>
#include <stddef.h>
#include "font8x16.h"

#define CHAR_W 8
#define CHAR_H 16

/* ---------- Multiboot info struct (only the fields we need) ---------- */

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint8_t  color_info[6];
} __attribute__((packed)) multiboot_info_t;

#define MULTIBOOT_FLAG_FRAMEBUFFER (1 << 12)

/* ---------- Framebuffer state ---------- */

static uint8_t *fb_addr;
static uint32_t fb_pitch;
static uint32_t fb_width;
static uint32_t fb_height;
static uint32_t fb_bpp_bytes;

static size_t term_cols;
static size_t term_rows;
static size_t term_col = 0;
static size_t term_row = 0;

/* Setting every byte of a pixel to 0xFF or 0x00 gives white/black under
 * any bit-packed RGB layout (RGB888, BGR888, RGB565, etc.) without needing
 * to know the exact channel order -- good enough for a monochrome shell. */
static inline void putpixel(uint32_t x, uint32_t y, uint8_t val) {
    if (x >= fb_width || y >= fb_height) return;
    uint8_t *p = fb_addr + y * fb_pitch + x * fb_bpp_bytes;
    for (uint32_t i = 0; i < fb_bpp_bytes; i++) {
        p[i] = val;
    }
}

static void fb_clear(void) {
    for (uint32_t y = 0; y < fb_height; y++) {
        uint8_t *row = fb_addr + y * fb_pitch;
        for (uint32_t x = 0; x < fb_pitch; x++) {
            row[x] = 0;
        }
    }
}

static void draw_char(size_t col, size_t row, char ch) {
    unsigned char code = (unsigned char)ch;
    if (code >= 128) code = '?';
    const uint8_t *glyph = font8x16[code];
    uint32_t px = (uint32_t)(col * CHAR_W);
    uint32_t py = (uint32_t)(row * CHAR_H);

    for (uint32_t gy = 0; gy < CHAR_H; gy++) {
        uint8_t bits = glyph[gy];
        for (uint32_t gx = 0; gx < CHAR_W; gx++) {
            int on = bits & (0x80 >> gx);
            putpixel(px + gx, py + gy, on ? 0xFF : 0x00);
        }
    }
}

static void term_scroll(void) {
    uint32_t row_bytes = fb_pitch * CHAR_H;
    uint8_t *dst = fb_addr;
    uint8_t *src = fb_addr + row_bytes;
    uint32_t total = fb_pitch * fb_height;

    for (uint32_t i = 0; i < total - row_bytes; i++) {
        dst[i] = src[i];
    }
    for (uint32_t i = total - row_bytes; i < total; i++) {
        fb_addr[i] = 0;
    }
    term_row = term_rows - 1;
}

static void term_putchar(char c) {
    if (c == '\n') {
        term_col = 0;
        term_row++;
    } else if (c == '\b') {
        if (term_col > 0) {
            term_col--;
            draw_char(term_col, term_row, ' ');
        }
    } else {
        draw_char(term_col, term_row, c);
        term_col++;
        if (term_col >= term_cols) {
            term_col = 0;
            term_row++;
        }
    }

    if (term_row >= term_rows) {
        term_scroll();
    }
}

static void term_print(const char *str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        term_putchar(str[i]);
    }
}

static void term_clear(void) {
    fb_clear();
    term_col = 0;
    term_row = 0;
}

/* ---------- Port I/O ---------- */

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ---------- PS/2 keyboard (polling, US QWERTY, scancode set 1) ---------- */

static const char scancode_to_ascii[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ',
};

static char keyboard_poll(void) {
    if (!(inb(0x64) & 1)) {
        return 0;
    }
    uint8_t scancode = inb(0x60);
    if (scancode & 0x80) {
        return 0;
    }
    if (scancode >= sizeof(scancode_to_ascii)) {
        return 0;
    }
    return scancode_to_ascii[scancode];
}

/* ---------- A tiny "shell" loop ---------- */

#define CMD_BUF_SIZE 128

static int str_eq(const char *a, const char *b) {
    size_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static void handle_command(const char *cmd) {
    if (cmd[0] == '\0') {
        return;
    }
    if (str_eq(cmd, "help")) {
        term_print("Available commands: help, clear, about\n");
    } else if (str_eq(cmd, "clear")) {
        term_clear();
    } else if (str_eq(cmd, "about")) {
        term_print("A tiny from-scratch OS. No Linux, no packages, just this kernel.\n");
    } else {
        term_print("Unknown command: ");
        term_print(cmd);
        term_print("\n");
    }
}

/* ---------- Entry point ---------- */

void kernel_main(uint32_t magic, uint32_t mbi_addr) {
    (void)magic; /* not checked -- if we got here, GRUB loaded us fine */

    multiboot_info_t *mbi = (multiboot_info_t *)(uintptr_t)mbi_addr;

    if (!(mbi->flags & MULTIBOOT_FLAG_FRAMEBUFFER)) {
        /* No framebuffer info available -- nothing we can draw with.
         * Halt; there is no display to report an error on. */
        while (1) { __asm__ volatile ("hlt"); }
    }

    fb_addr = (uint8_t *)(uintptr_t)mbi->framebuffer_addr;
    fb_pitch = mbi->framebuffer_pitch;
    fb_width = mbi->framebuffer_width;
    fb_height = mbi->framebuffer_height;
    fb_bpp_bytes = mbi->framebuffer_bpp / 8;
    if (fb_bpp_bytes == 0) fb_bpp_bytes = 4;

    term_cols = fb_width / CHAR_W;
    term_rows = fb_height / CHAR_H;

    term_clear();
    term_print("Custom 32-bit OS booted.\n");
    term_print("Type 'help' for a list of commands.\n\n");
    term_print("> ");

    char cmd_buf[CMD_BUF_SIZE];
    size_t cmd_len = 0;

    while (1) {
        char c = keyboard_poll();
        if (c == 0) {
            continue;
        }

        if (c == '\n') {
            cmd_buf[cmd_len] = '\0';
            term_putchar('\n');
            handle_command(cmd_buf);
            cmd_len = 0;
            term_print("> ");
        } else if (c == '\b') {
            if (cmd_len > 0) {
                cmd_len--;
                term_putchar('\b');
            }
        } else if (cmd_len < CMD_BUF_SIZE - 1) {
            cmd_buf[cmd_len++] = c;
            term_putchar(c);
        }
    }
}
