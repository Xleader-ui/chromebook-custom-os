/* kernel.c -- the actual operating system.
 *
 * Everything here is written from scratch: no Linux, no libc, nothing
 * downloaded at build time except the compiler itself. This file talks
 * directly to VGA text-mode memory and the PS/2 keyboard controller.
 */

#include <stdint.h>
#include <stddef.h>

/* ---------- VGA text mode output ---------- */

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)

static size_t term_row = 0;
static size_t term_col = 0;
static uint8_t term_color = 0x0F; /* white on black */

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

static void term_clear(void) {
    volatile uint16_t *vga = VGA_MEMORY;
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga[y * VGA_WIDTH + x] = vga_entry(' ', term_color);
        }
    }
    term_row = 0;
    term_col = 0;
}

static void term_scroll(void) {
    volatile uint16_t *vga = VGA_MEMORY;
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga[(y - 1) * VGA_WIDTH + x] = vga[y * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', term_color);
    }
    term_row = VGA_HEIGHT - 1;
}

static void term_putchar(char c) {
    volatile uint16_t *vga = VGA_MEMORY;

    if (c == '\n') {
        term_col = 0;
        term_row++;
    } else if (c == '\b') {
        if (term_col > 0) {
            term_col--;
            vga[term_row * VGA_WIDTH + term_col] = vga_entry(' ', term_color);
        }
    } else {
        vga[term_row * VGA_WIDTH + term_col] = vga_entry(c, term_color);
        term_col++;
        if (term_col >= VGA_WIDTH) {
            term_col = 0;
            term_row++;
        }
    }

    if (term_row >= VGA_HEIGHT) {
        term_scroll();
    }
}

static void term_print(const char *str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        term_putchar(str[i]);
    }
}

/* ---------- Port I/O ---------- */

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* ---------- PS/2 keyboard (polling, US QWERTY, scancode set 1) ---------- */

static const char scancode_to_ascii[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ',
    /* rest unused for this simple shell */
};

static char keyboard_poll(void) {
    if (!(inb(0x64) & 1)) {
        return 0; /* nothing waiting */
    }
    uint8_t scancode = inb(0x60);
    if (scancode & 0x80) {
        return 0; /* key release, ignore */
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

void kernel_main(void) {
    term_clear();
    term_print("Custom 32-bit OS booted.\n");
    term_print("Type 'help' for a list of commands.\n\n");
    term_print("> ");

    char cmd_buf[CMD_BUF_SIZE];
    size_t cmd_len = 0;

    while (1) {
        char c = keyboard_poll();
        if (c == 0) {
            continue; /* busy-wait for the next keystroke */
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
