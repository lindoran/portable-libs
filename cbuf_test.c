/* cbuf_test.c -- validation and demonstration for cbuf
 *
 * Build default (128-byte table, runtime scroll):
 *   gcc -std=c89 -Wall cbuf_test.c cbuf.c -o t_default
 *
 * Build ASCII native (no table):
 *   gcc -std=c89 -Wall -DCBUF_ASCII_NATIVE cbuf_test.c cbuf.c -o t_native
 *
 * Build 256-byte table:
 *   gcc -std=c89 -Wall -DCBUF_TABLE_SIZE=256 cbuf_test.c cbuf.c -o t_256
 *
 * Build table pointer + copy:
 *   gcc -std=c89 -Wall -DCBUF_TABLE_POINTER cbuf_test.c cbuf.c -o t_ptr
 *
 * Build no scroll:
 *   gcc -std=c89 -Wall -DCBUF_NO_SCROLL cbuf_test.c cbuf.c -o t_noscroll
 *
 * Build 256 + pointer (CoCo-style):
 *   gcc -std=c89 -Wall -DCBUF_TABLE_SIZE=256 -DCBUF_TABLE_POINTER cbuf_test.c cbuf.c -o t_coco
 */

#include <stdio.h>
#include "cbuf.h"

/* Test buffer: 16 cols x 8 rows = 128 bytes */
#define TEST_COLS  16
#define TEST_ROWS   8

static uint8_t screen[TEST_COLS * TEST_ROWS];

/* -----------------------------------------------------------------------
 * Dump the buffer as ASCII text with a border -- makes the display visible
 * in the test output without any hardware
 * ----------------------------------------------------------------------- */
static void dump(const char *label)
{
    uint8_t x, y;
    printf("\n%s  (cursor at %d,%d)\n", label, cbuf_get_x(), cbuf_get_y());
    printf("+");
    for (x = 0; x < TEST_COLS; x++) printf("-");
    printf("+\n");
    for (y = 0; y < TEST_ROWS; y++) {
        printf("|");
        for (x = 0; x < TEST_COLS; x++) {
            uint8_t c = cbuf_get_raw(x, y);
            /* Print printable chars, dot for control codes */
            printf("%c", (c >= 32 && c < 127) ? (char)c : '.');
        }
        printf("|\n");
    }
    printf("+");
    for (x = 0; x < TEST_COLS; x++) printf("-");
    printf("+\n");
}

/* =======================================================================
 * Test 1: basic init and clear
 * ======================================================================= */
static void test_init(void)
{
    printf("=== test_init ===\n");
    cbuf_init(screen, TEST_COLS, TEST_ROWS);
    cbuf_clear(' ');
    dump("after clear(' ')");

    /* Cursor should be at 0,0 after clear */
    printf("cursor x=%d (expect 0)\n", cbuf_get_x());
    printf("cursor y=%d (expect 0)\n", cbuf_get_y());
}

/* =======================================================================
 * Test 2: cbuf_goto and cbuf_putc
 * ======================================================================= */
static void test_goto_putc(void)
{
    printf("\n=== test_goto_putc ===\n");
    cbuf_init(screen, TEST_COLS, TEST_ROWS);
    cbuf_clear(' ');

    cbuf_goto(0, 0);
    cbuf_puts("Hello, world!");

    cbuf_goto(2, 2);
    cbuf_putc('A');
    cbuf_putc('B');
    cbuf_putc('C');

    cbuf_goto(TEST_COLS - 3, TEST_ROWS - 1);
    cbuf_puts("END");

    dump("hello + ABC + END");
}

/* =======================================================================
 * Test 3: out of bounds goto clamps and returns CBUF_OVERFLOW
 * ======================================================================= */
static void test_bounds(void)
{
    int8_t rc;
    printf("\n=== test_bounds ===\n");
    cbuf_init(screen, TEST_COLS, TEST_ROWS);
    cbuf_clear(' ');

    rc = cbuf_goto(TEST_COLS + 5, TEST_ROWS + 5);
    printf("goto(out of bounds) returned %d (expect %d)\n",
           (int)rc, (int)CBUF_OVERFLOW);
    printf("cursor clamped to %d,%d (expect %d,%d)\n",
           cbuf_get_x(), cbuf_get_y(),
           TEST_COLS - 1, TEST_ROWS - 1);
}

/* =======================================================================
 * Test 4: cbuf_clear_line and cbuf_clear_eol
 * ======================================================================= */
static void test_clear_ops(void)
{
    printf("\n=== test_clear_ops ===\n");
    cbuf_init(screen, TEST_COLS, TEST_ROWS);
    cbuf_clear('.');

    cbuf_clear_line(3, '-');
    dump("clear_line(3, '-')");

    cbuf_goto(4, 5);
    cbuf_clear_eol('*');
    dump("goto(4,5) clear_eol('*')");
}

/* =======================================================================
 * Test 5: cbuf_put_raw and cbuf_get_raw
 * ======================================================================= */
static void test_raw(void)
{
    int8_t  rc;
    uint8_t v;
    printf("\n=== test_raw ===\n");
    cbuf_init(screen, TEST_COLS, TEST_ROWS);
    cbuf_clear(' ');

    /* Write a raw screen code directly -- no translation */
    cbuf_put_raw(5, 3, (uint8_t)'X');
    v = cbuf_get_raw(5, 3);
    printf("put_raw/get_raw at (5,3): got '%c' (expect 'X')\n", (char)v);

    /* Out of bounds raw write */
    rc = cbuf_put_raw(TEST_COLS, 0, 'Z');
    printf("put_raw(out of bounds) returned %d (expect %d)\n",
           (int)rc, (int)CBUF_OVERFLOW);

    /* get_raw returns 0 out of bounds */
    v = cbuf_get_raw(TEST_COLS, 0);
    printf("get_raw(out of bounds) returned %d (expect 0)\n", (int)v);
}

/* =======================================================================
 * Test 6: cbuf_getc and cbuf_read_eol
 * ======================================================================= */
static void test_read(void)
{
    char    buf[TEST_COLS + 1];
    uint8_t n;
    printf("\n=== test_read ===\n");
    cbuf_init(screen, TEST_COLS, TEST_ROWS);
    cbuf_clear(' ');

    cbuf_goto(0, 2);
    cbuf_puts("Hello   ");   /* trailing spaces are fill */

    /* getc reads what's under the cursor */
    cbuf_goto(0, 2);
    printf("getc at (0,2): '%c' (expect 'H')\n", (char)cbuf_getc());

    /* read_eol strips trailing fill */
    cbuf_goto(0, 2);
    n = cbuf_read_eol(' ', buf, (uint8_t)sizeof(buf));
    printf("read_eol: \"%s\" len=%d (expect \"Hello\" len=5)\n", buf, (int)n);

    /* read_eol from middle of content */
    cbuf_goto(2, 2);
    n = cbuf_read_eol(' ', buf, (uint8_t)sizeof(buf));
    printf("read_eol from col 2: \"%s\" (expect \"llo\")\n", buf);
}

/* =======================================================================
 * Test 7: scroll
 * ======================================================================= */
#ifndef CBUF_NO_SCROLL
static void test_scroll(void)
{
    uint8_t y;
    int8_t  rc;
    printf("\n=== test_scroll ===\n");
    cbuf_init(screen, TEST_COLS, TEST_ROWS);
    cbuf_clear(' ');
    cbuf_set_scroll(1);

    /* Fill every row with a label */
    for (y = 0; y < TEST_ROWS; y++) {
        cbuf_goto(0, y);
        cbuf_putc((uint8_t)('0' + y));
        cbuf_putc(':');
        cbuf_puts("row content");
    }
    dump("before scroll");

    /* Force a scroll by writing a full line past the last row.
     * Put cursor at col 0 of last row and write more than one row.  */
    cbuf_goto(0, TEST_ROWS - 1);
    rc = cbuf_puts("SCROLL TRIGGER LINE!!");  /* >16 chars forces wrap+scroll */
    printf("puts past last row returned %d (expect %d=SCROLLED)\n",
           (int)rc, (int)CBUF_SCROLLED);
    dump("after overflow scroll");

    /* Explicit scroll_up */
    cbuf_scroll_up('~');
    dump("after explicit scroll_up('~')");
}
#endif

/* =======================================================================
 * Test 8: scroll OFF -- hard stop
 * ======================================================================= */
static void test_no_scroll_runtime(void)
{
#ifndef CBUF_NO_SCROLL
    int8_t rc;
    printf("\n=== test_no_scroll (runtime off) ===\n");
    cbuf_init(screen, TEST_COLS, TEST_ROWS);
    cbuf_clear(' ');
    cbuf_set_scroll(0);   /* hard stop */

    cbuf_goto(TEST_COLS - 3, TEST_ROWS - 1);
    cbuf_puts("AB");

    /* This should overflow without scrolling */
    rc = cbuf_puts("OVERFLOW");
    printf("puts past end returned %d (expect %d=OVERFLOW)\n",
           (int)rc, (int)CBUF_OVERFLOW);
    dump("hard stop -- last chars not written");
#else
    printf("\n=== test_no_scroll (compile-time CBUF_NO_SCROLL) ===\n");
    {
        int8_t rc;
        cbuf_init(screen, TEST_COLS, TEST_ROWS);
        cbuf_clear(' ');
        cbuf_goto(TEST_COLS - 2, TEST_ROWS - 1);
        cbuf_puts("AB");
        rc = cbuf_puts("OVERFLOW");
        printf("puts past end returned %d (expect %d=OVERFLOW)\n",
               (int)rc, (int)CBUF_OVERFLOW);
        dump("CBUF_NO_SCROLL hard stop");
    }
#endif
}

/* =======================================================================
 * Test 9: table pointer and copy (if enabled)
 * ======================================================================= */
#ifdef CBUF_TABLE_POINTER
static void test_table_pointer(void)
{
    static uint8_t my_table[CBUF_TABLE_SIZE];
    uint8_t v;

    printf("\n=== test_table_pointer ===\n");
    cbuf_init(screen, TEST_COLS, TEST_ROWS);
    cbuf_clear(' ');

    /* Clone the ROM default and patch one entry */
    cbuf_copy_table(my_table);
    my_table[(uint8_t)'A'] = (uint8_t)'a';   /* remap 'A' -> 'a' */
    cbuf_set_table(my_table);

    cbuf_goto(0, 0);
    cbuf_putc('A');   /* should write 'a' due to patched table */
    cbuf_putc('B');   /* should write 'B' unchanged */

    v = cbuf_get_raw(0, 0);
    printf("putc('A') with patched table: got '%c' (expect 'a')\n", (char)v);
    v = cbuf_get_raw(1, 0);
    printf("putc('B') with default:       got '%c' (expect 'B')\n", (char)v);

    /* Repoint back to ROM default */
    cbuf_set_table(cbuf_default_table);
    cbuf_goto(2, 0);
    cbuf_putc('A');
    v = cbuf_get_raw(2, 0);
    printf("after repoint to ROM: putc('A') got '%c' (expect 'A')\n", (char)v);
}
#endif

/* =======================================================================
 * Test 10: blink cursor pattern (application-level, uses put_raw)
 * Demonstrates palette philosophy -- the library provides put_raw,
 * the application builds the blink from it.
 * ======================================================================= */
static void test_cursor_pattern(void)
{
    uint8_t saved;
    printf("\n=== test_cursor_pattern (app-level blink demo) ===\n");
    cbuf_init(screen, TEST_COLS, TEST_ROWS);
    cbuf_clear(' ');
    cbuf_goto(5, 3);
    cbuf_puts("edit here");

    /* Save char under cursor, replace with block, restore */
    saved = cbuf_get_raw(cbuf_get_x(), cbuf_get_y());
    cbuf_put_raw(cbuf_get_x(), cbuf_get_y(), (uint8_t)0xDB); /* block char */
    dump("cursor shown as block (0xDB)");

    cbuf_put_raw(cbuf_get_x(), cbuf_get_y(), saved);
    dump("cursor restored");
    printf("(Application toggles between these two states on timer tick)\n");
}

/* =======================================================================
 * Banner
 * ======================================================================= */
static void banner(void)
{
    printf("cbuf character display buffer test\n");
    printf("  Buffer     : %d cols x %d rows = %d bytes\n",
           TEST_COLS, TEST_ROWS, TEST_COLS * TEST_ROWS);
#ifdef CBUF_ASCII_NATIVE
    printf("  Table      : ASCII native (no translation)\n");
#elif defined(CBUF_TABLE_POINTER)
    printf("  Table      : runtime pointer (CBUF_TABLE_POINTER)\n");
    printf("  Table size : %d bytes\n", CBUF_TABLE_SIZE);
#else
    printf("  Table      : ROM default, %d bytes\n", CBUF_TABLE_SIZE);
#endif
#ifdef CBUF_NO_SCROLL
    printf("  Scroll     : compile-time dropped (CBUF_NO_SCROLL)\n");
#else
    printf("  Scroll     : runtime switchable\n");
#endif
    printf("\n");
}

int main(void)
{
    banner();
    test_init();
    test_goto_putc();
    test_bounds();
    test_clear_ops();
    test_raw();
    test_read();
#ifndef CBUF_NO_SCROLL
    test_scroll();
#endif
    test_no_scroll_runtime();
#ifdef CBUF_TABLE_POINTER
    test_table_pointer();
#endif
    test_cursor_pattern();

    printf("\nDone.\n");
    return 0;
}
