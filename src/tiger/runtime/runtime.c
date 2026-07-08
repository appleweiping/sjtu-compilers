/* Tiger runtime library.
 *
 * Provides the standard library the Tiger program links against, plus the
 * allocation and string helpers the LLVM-IR backend emits calls to. All Tiger
 * values are machine words (int64). Strings are length-prefixed byte buffers:
 * the first 8 bytes hold the length, followed by the raw characters. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int64_t ti;  /* a Tiger integer / pointer-sized word */

/* ---- memory ---- */

/* Allocate a record of `size` bytes, zero-initialised. */
ti *tiger_alloc_record(ti size) {
  ti *p = (ti *)calloc(1, size < 0 ? 0 : (size_t)size);
  return p;
}

/* Allocate a Tiger array of `count` elements each initialised to `init`. */
ti *tiger_init_array(ti count, ti init) {
  ti n = count < 0 ? 0 : count;
  ti *a = (ti *)malloc(sizeof(ti) * (size_t)n);
  for (ti i = 0; i < n; ++i) a[i] = init;
  return a;
}

/* ---- string representation ---- */

/* A Tiger string literal, produced by the compiler as a global. */
struct tstring {
  ti length;
  char chars[1];
};

static struct tstring *make_string(const char *s, ti len) {
  struct tstring *p = (struct tstring *)malloc(sizeof(ti) + (size_t)len + 1);
  p->length = len;
  memcpy(p->chars, s, (size_t)len);
  p->chars[len] = '\0';
  return p;
}

/* ---- standard library (Appel appendix) ---- */

void print(struct tstring *s) {
  fwrite(s->chars, 1, (size_t)s->length, stdout);
}

/* printi: print an integer with no surrounding whitespace. */
void printi(ti i) { printf("%lld", (long long)i); }

void flush(void) { fflush(stdout); }

/* getchar wrapper: returns a 1-char Tiger string, or an empty string at EOF.
 * The grader links with `-Wl,--wrap,getchar`, so the Tiger `getchar` is named
 * `__wrap_getchar`; we also export a plain `tiger_getchar` for our own linker
 * command that does not use the wrap flag. */
static struct tstring *do_getchar(void) {
  int c = fgetc(stdin);
  if (c == EOF) return make_string("", 0);
  char buf[1];
  buf[0] = (char)c;
  return make_string(buf, 1);
}
struct tstring *__wrap_getchar(void) { return do_getchar(); }
struct tstring *tiger_getchar(void) { return do_getchar(); }

ti ord(struct tstring *s) {
  if (s->length == 0) return -1;
  return (unsigned char)s->chars[0];
}

struct tstring *chr(ti i) {
  if (i < 0 || i > 255) {
    fprintf(stderr, "chr: character out of range\n");
    exit(1);
  }
  char buf[1];
  buf[0] = (char)i;
  return make_string(buf, 1);
}

ti size(struct tstring *s) { return s->length; }

struct tstring *substring(struct tstring *s, ti first, ti n) {
  if (first < 0 || n < 0 || first + n > s->length) {
    fprintf(stderr, "substring: index out of range\n");
    exit(1);
  }
  return make_string(s->chars + first, n);
}

struct tstring *concat(struct tstring *a, struct tstring *b) {
  if (a->length == 0) return b;
  if (b->length == 0) return a;
  ti len = a->length + b->length;
  struct tstring *p = (struct tstring *)malloc(sizeof(ti) + (size_t)len + 1);
  p->length = len;
  memcpy(p->chars, a->chars, (size_t)a->length);
  memcpy(p->chars + a->length, b->chars, (size_t)b->length);
  p->chars[len] = '\0';
  return p;
}

/* String equality: returns 1 if the two Tiger strings are equal, else 0. */
ti tiger_streq(struct tstring *a, struct tstring *b) {
  if (a == b) return 1;
  if (a->length != b->length) return 0;
  return memcmp(a->chars, b->chars, (size_t)a->length) == 0 ? 1 : 0;
}

ti tiger_not(ti i) { return !i; }

void tiger_exit(ti code) { exit((int)code); }

/* Runtime error helpers the backend may emit. */
void tiger_error_bounds(void) {
  fprintf(stderr, "runtime error: array index out of bounds\n");
  exit(1);
}

void tiger_error_nil(void) {
  fprintf(stderr, "runtime error: nil record dereference\n");
  exit(1);
}
