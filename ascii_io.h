/*
 * ascii_io.h  --  Portable ASCII console I/O, no stdlib
 *
 * Standard : ANSI C89 / C90
 * Targets  : GCC, ia16-gcc, MinGW, SDCC, Z88DK (sccz80 + SDCC), Arduino
 *
 * Integer scope: uint8_t / int8_t / uint16_t / int16_t only.
 * Every uint16_t value (0-65535) is exactly representable in IEEE 754
 * single-precision float, so float output covers the full integer range
 * and beyond (exact to ~16.7M, range to ~3.4e38).
 *
 * -----------------------------------------------------------------------
 * FEATURE DROP FLAGS  (define before including to shrink the binary)
 * -----------------------------------------------------------------------
 *  ASCII_NO_FLOAT   -- omit all float support (ascii_put_f32)
 *                      Safe to set when using pmath in fixed-point mode.
 *                      Must NOT be set when PMATH_USE_FLOAT is defined;
 *                      pmath.h will emit a #error if both are active.
 *  ASCII_NO_BCD     -- omit packed-BCD output
 *  ASCII_NO_INPUT   -- omit all input and parse functions
 *  ASCII_NO_STDINT  -- omit <stdint.h>; you supply the fallback typedefs
 *                      (see PORTABLE_TYPES_DEFINED note below)
 *  ASCII_CRLF       -- ascii_nl() emits CR+LF instead of LF only
 *  ASCII_GETS_ECHO  -- ascii_gets() echoes characters as typed
 *
 * -----------------------------------------------------------------------
 * PLATFORM HOOKS  (define in your platform .c file)
 * -----------------------------------------------------------------------
 *  void ascii_putc(char c);   -- required always
 *  char ascii_getc(void);     -- required unless ASCII_NO_INPUT
 *
 * To inline your functions with no wrapper overhead, define the macro
 * forms BEFORE including this header:
 *  #define ASCII_PUTC(c)   my_uart_tx(c)
 *  #define ASCII_GETC()    my_uart_rx()
 *
 * -----------------------------------------------------------------------
 * K&R PORTING NOTES
 * -----------------------------------------------------------------------
 *  - Define ASCII_NO_STDINT and provide typedefs manually
 *  - Remove the extern prototypes; declare them K&R style in each .c
 *  - Replace ASCII_CONST with nothing (K&R has no const)
 *  - Remove the __cplusplus guards if your preprocessor rejects them
 */

#ifndef ASCII_IO_H
#define ASCII_IO_H

#ifdef __cplusplus
extern "C" {
#endif

/* const compatibility --------------------------------------------------- */
#ifdef __STDC__
#  define ASCII_CONST const
#else
#  define ASCII_CONST
#endif

/* -----------------------------------------------------------------------
 * INTEGER TYPES
 *
 * PORTABLE_TYPES_DEFINED is a shared sentinel used by both ascii_io.h and
 * pmath.h.  Whichever header is included first defines the types; the
 * second skips its block entirely, preventing redefinition errors when
 * both ASCII_NO_STDINT and PMATH_NO_STDINT are set in the same build.
 * ----------------------------------------------------------------------- */
#ifndef PORTABLE_TYPES_DEFINED
#  define PORTABLE_TYPES_DEFINED
#  ifndef ASCII_NO_STDINT
#    include <stdint.h>
#  else
     /* Adjust for your platform if the defaults are wrong. */
     typedef unsigned char  uint8_t;
     typedef signed   char  int8_t;
     typedef unsigned short uint16_t;
     typedef signed   short int16_t;
#  endif
#endif

/* NULL without stddef.h ------------------------------------------------- */
#ifndef NULL
#  ifdef __cplusplus
#    define NULL 0
#  else
#    define NULL ((void *)0)
#  endif
#endif

/* Output hook ----------------------------------------------------------- */
#ifndef ASCII_PUTC
extern void ascii_putc(char c);
#  define ASCII_PUTC(c)  ascii_putc(c)
#endif

/* Input hook ------------------------------------------------------------ */
#ifndef ASCII_NO_INPUT
#  ifndef ASCII_GETC
extern char ascii_getc(void);
#    define ASCII_GETC()  ascii_getc()
#  endif
#endif

/* =======================================================================
 * STRING OUTPUT
 * ======================================================================= */

void ascii_puts(ASCII_CONST char *s);        /* NUL-terminated, NULL-safe  */
void ascii_putn(ASCII_CONST char *s, uint8_t n); /* exactly n bytes        */
void ascii_nl(void);                         /* LF, or CR+LF if ASCII_CRLF */

/* =======================================================================
 * HEX OUTPUT  (zero-padded, uppercase)
 * ======================================================================= */

void ascii_put_x8 (uint8_t  v);   /* "FF"   */
void ascii_put_x16(uint16_t v);   /* "FFFF" */

void ascii_put_x8p (uint8_t  v);  /* "0xFF"   */
void ascii_put_x16p(uint16_t v);  /* "0xFFFF" */

/* =======================================================================
 * UNSIGNED DECIMAL OUTPUT
 * ======================================================================= */

void ascii_put_u8 (uint8_t  v);   /*   0..255   */
void ascii_put_u16(uint16_t v);   /*   0..65535 */

/* =======================================================================
 * SIGNED DECIMAL OUTPUT
 * ======================================================================= */

void ascii_put_i8 (int8_t  v);    /* -128..127   */
void ascii_put_i16(int16_t v);    /* -32768..32767 */

/* =======================================================================
 * FLOAT OUTPUT
 *
 * ascii_put_f32(v, decimals) -- decimals capped at 7 (float precision).
 * Whole part exact to 16,777,216; ~7 significant digits beyond that.
 * Rounding: half-up applied before splitting whole/fractional parts.
 * NaN and Inf not handled.
 * SDCC/Z88DK: software float is large; use ASCII_NO_FLOAT when possible.
 *
 * Note: do not define ASCII_NO_FLOAT when PMATH_USE_FLOAT is active.
 * pmath.h enforces this with a #error.  In fixed-point mode, pm_print()
 * uses integer arithmetic only and ASCII_NO_FLOAT is safe.
 * ======================================================================= */

#ifndef ASCII_NO_FLOAT
void ascii_put_f32(float v, uint8_t decimals);
#endif

/* =======================================================================
 * PACKED BCD OUTPUT
 *
 * Each nibble = one decimal digit 0-9.  No validity check on nibbles.
 *   ascii_put_bcd8(0x42)    --> "42"
 *   ascii_put_bcd16(0x1999) --> "1999"
 * ======================================================================= */

#ifndef ASCII_NO_BCD
void ascii_put_bcd8 (uint8_t  v);
void ascii_put_bcd16(uint16_t v);
#endif

/* =======================================================================
 * STRING INPUT
 * ======================================================================= */

#ifndef ASCII_NO_INPUT

/*
 * Read up to (maxlen-1) chars, NUL-terminate, return char count.
 * Stops on CR or LF.  Handles backspace / DEL (0x7F).
 * Define ASCII_GETS_ECHO to echo each character as typed.
 */
uint8_t ascii_gets(char *buf, uint8_t maxlen);

/* =======================================================================
 * PARSE  (string -> number)
 *
 * Returns 1 on success, 0 on failure (bad format or overflow).
 * *out is unchanged on failure.
 * Stops at the first character that does not belong to the number.
 * Leading whitespace is NOT skipped; strip it first if needed.
 * ascii_parse_x*: accepts optional leading "0x" or "0X".
 * ascii_parse_i*: accepts optional leading '+' or '-'.
 * ======================================================================= */

uint8_t ascii_parse_u8 (ASCII_CONST char *s, uint8_t  *out);
uint8_t ascii_parse_u16(ASCII_CONST char *s, uint16_t *out);
uint8_t ascii_parse_i8 (ASCII_CONST char *s, int8_t   *out);
uint8_t ascii_parse_i16(ASCII_CONST char *s, int16_t  *out);
uint8_t ascii_parse_x8 (ASCII_CONST char *s, uint8_t  *out);
uint8_t ascii_parse_x16(ASCII_CONST char *s, uint16_t *out);

/* =======================================================================
 * STREAM READ  (reads directly from ASCII_GETC hook)
 *
 * Skips leading spaces and tabs.
 * Reads until a character that does not belong to the number; that
 * terminating character is consumed (usually CR/LF -- harmless on a
 * line-oriented terminal).
 * Returns 1 on success, 0 on failure.
 * ======================================================================= */

uint8_t ascii_read_u16(uint16_t *out);
uint8_t ascii_read_i16(int16_t  *out);
uint8_t ascii_read_x16(uint16_t *out);

#endif /* ASCII_NO_INPUT */

#ifdef __cplusplus
}
#endif

#endif /* ASCII_IO_H */
