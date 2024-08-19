#ifndef TERMINIZER_H
#define TERMINIZER_H

#include <stdint.h>

#define TZ_MAX_COLS 512
#define TZ_MAX_ROWS 256

struct tz_vertex {
  union {
    struct {
      float x;
      float y;
      float z;
      float w;
    };

    float pos[4];
  };

  uint8_t r;
  uint8_t g;
  uint8_t b;
};

void tz_init(int w, int h);

int tz_width();
int tz_height();

/* output routines */
void tz_viewport(int x, int y, int w, int h);

void tz_clear();

int tz_print(int x, int y, const char *fmt, ...);
void tz_blit(int x, int y, int w, int h, const uint32_t *data);

void tz_line(const struct tz_vertex *v0, const struct tz_vertex *v1);
void tz_triangle(const struct tz_vertex *v0, const struct tz_vertex *v1, const struct tz_vertex *v2);

void tz_paint();

/* input routines */
int tz_can_read();
int tz_read(char *out, int n);

int tz_prompt(int y, const char *prompt, char *out, int n);

#endif

#ifdef TERMINIZER_IMPLEMENTATION

#include <ctype.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define TZ_MIN(a, b)        (((a) < (b)) ? (a) : (b))
#define TZ_MAX(a, b)        (((a) > (b)) ? (a) : (b))
#define TZ_CLAMP(x, lo, hi) TZ_MAX((lo), TZ_MIN((hi), (x)))

#define TZ_SUBPIXEL_BITS    4
#define TZ_SUBPIXEL_STEP    (1 << TZ_SUBPIXEL_BITS)
#define TZ_SUBPIXEL_MASK    (TZ_SUBPIXEL_STEP - 1)

static struct {
  struct termios old_tty;
  struct sigaction old_sa;

  int rows;
  int cols;
  int y;

  int x0, y0;
  int x1, y1;

  uint32_t fg_color;
  uint32_t bg_color;

  uint64_t dirty[TZ_MAX_ROWS][TZ_MAX_COLS / 64];

  uint32_t color[TZ_MAX_ROWS << 1][TZ_MAX_COLS];
  uint8_t depth[TZ_MAX_ROWS << 1][TZ_MAX_COLS];
  char chars[TZ_MAX_ROWS][TZ_MAX_COLS];
} tz;

static const uint32_t ansi_lut[256] = {
    0x00000000, 0x00000080, 0x00008000, 0x00008080, 0x00800000, 0x00800080, 0x00808000, 0x00c0c0c0, 0x00808080,
    0x000000ff, 0x0000ff00, 0x0000ffff, 0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x00ffffff, 0x00000000, 0x005f0000,
    0x00870000, 0x00af0000, 0x00d70000, 0x00ff0000, 0x00005f00, 0x005f5f00, 0x00875f00, 0x00af5f00, 0x00d75f00,
    0x00ff5f00, 0x00008700, 0x005f8700, 0x00878700, 0x00af8700, 0x00d78700, 0x00ff8700, 0x0000af00, 0x005faf00,
    0x0087af00, 0x00afaf00, 0x00d7af00, 0x00ffaf00, 0x0000d700, 0x005fd700, 0x0087d700, 0x00afd700, 0x00d7d700,
    0x00ffd700, 0x0000ff00, 0x005fff00, 0x0087ff00, 0x00afff00, 0x00d7ff00, 0x00ffff00, 0x0000005f, 0x005f005f,
    0x0087005f, 0x00af005f, 0x00d7005f, 0x00ff005f, 0x00005f5f, 0x005f5f5f, 0x00875f5f, 0x00af5f5f, 0x00d75f5f,
    0x00ff5f5f, 0x0000875f, 0x005f875f, 0x0087875f, 0x00af875f, 0x00d7875f, 0x00ff875f, 0x0000af5f, 0x005faf5f,
    0x0087af5f, 0x00afaf5f, 0x00d7af5f, 0x00ffaf5f, 0x0000d75f, 0x005fd75f, 0x0087d75f, 0x00afd75f, 0x00d7d75f,
    0x00ffd75f, 0x0000ff5f, 0x005fff5f, 0x0087ff5f, 0x00afff5f, 0x00d7ff5f, 0x00ffff5f, 0x00000087, 0x005f0087,
    0x00870087, 0x00af0087, 0x00d70087, 0x00ff0087, 0x00005f87, 0x005f5f87, 0x00875f87, 0x00af5f87, 0x00d75f87,
    0x00ff5f87, 0x00008787, 0x005f8787, 0x00878787, 0x00af8787, 0x00d78787, 0x00ff8787, 0x0000af87, 0x005faf87,
    0x0087af87, 0x00afaf87, 0x00d7af87, 0x00ffaf87, 0x0000d787, 0x005fd787, 0x0087d787, 0x00afd787, 0x00d7d787,
    0x00ffd787, 0x0000ff87, 0x005fff87, 0x0087ff87, 0x00afff87, 0x00d7ff87, 0x00ffff87, 0x000000af, 0x005f00af,
    0x008700af, 0x00af00af, 0x00d700af, 0x00ff00af, 0x00005faf, 0x005f5faf, 0x00875faf, 0x00af5faf, 0x00d75faf,
    0x00ff5faf, 0x000087af, 0x005f87af, 0x008787af, 0x00af87af, 0x00d787af, 0x00ff87af, 0x0000afaf, 0x005fafaf,
    0x0087afaf, 0x00afafaf, 0x00d7afaf, 0x00ffafaf, 0x0000d7af, 0x005fd7af, 0x0087d7af, 0x00afd7af, 0x00d7d7af,
    0x00ffd7af, 0x0000ffaf, 0x005fffaf, 0x0087ffaf, 0x00afffaf, 0x00d7ffaf, 0x00ffffaf, 0x000000d7, 0x005f00d7,
    0x008700d7, 0x00af00d7, 0x00d700d7, 0x00ff00d7, 0x00005fd7, 0x005f5fd7, 0x00875fd7, 0x00af5fd7, 0x00d75fd7,
    0x00ff5fd7, 0x000087d7, 0x005f87d7, 0x008787d7, 0x00af87d7, 0x00d787d7, 0x00ff87d7, 0x0000afd7, 0x005fafd7,
    0x0087afd7, 0x00afafd7, 0x00d7afd7, 0x00ffafd7, 0x0000d7d7, 0x005fd7d7, 0x0087d7d7, 0x00afd7d7, 0x00d7d7d7,
    0x00ffd7d7, 0x0000ffd7, 0x005fffd7, 0x0087ffd7, 0x00afffd7, 0x00d7ffd7, 0x00ffffd7, 0x000000ff, 0x005f00ff,
    0x008700ff, 0x00af00ff, 0x00d700ff, 0x00ff00ff, 0x00005fff, 0x005f5fff, 0x00875fff, 0x00af5fff, 0x00d75fff,
    0x00ff5fff, 0x000087ff, 0x005f87ff, 0x008787ff, 0x00af87ff, 0x00d787ff, 0x00ff87ff, 0x0000afff, 0x005fafff,
    0x0087afff, 0x00afafff, 0x00d7afff, 0x00ffafff, 0x0000d7ff, 0x005fd7ff, 0x0087d7ff, 0x00afd7ff, 0x00d7d7ff,
    0x00ffd7ff, 0x0000ffff, 0x005fffff, 0x0087ffff, 0x00afffff, 0x00d7ffff, 0x00ffffff, 0x00080808, 0x00121212,
    0x001c1c1c, 0x00262626, 0x00303030, 0x003a3a3a, 0x00444444, 0x004e4e4e, 0x00585858, 0x00626262, 0x006c6c6c,
    0x00767676, 0x00808080, 0x008a8a8a, 0x00949494, 0x009e9e9e, 0x00a8a8a8, 0x00b2b2b2, 0x00bcbcbc, 0x00c6c6c6,
    0x00d0d0d0, 0x00dadada, 0x00e4e4e4, 0x00eeeeee,
};

#ifdef _MSC_VER

#include <intrin.h>

static int tz_ctz64(uint64_t x) {
  unsigned long r = 0;
  if (!_BitScanForward64(&r, v)) {
    return 64;
  }
  return r;
}

#else

static inline int tz_ctz64(uint64_t v) {
  return v ? __builtin_ctzll(v) : 64;
}

#endif

static uint8_t tz_clamp_u8(int c) {
  return TZ_CLAMP(c, 0x00, 0xff);
}

static uint8_t tz_blue(uint32_t color) {
  return (color >> 16) & 0xff;
}

static uint8_t tz_green(uint32_t color) {
  return (color >> 8) & 0xff;
}

static uint8_t tz_red(uint32_t color) {
  return color & 0xff;
}

static uint32_t tz_color(uint8_t r, uint8_t g, uint8_t b) {
  return (b << 16) | (g << 8) | r;
}

static void tz_write(const char *fmt, ...) {
  char buf[TZ_MAX_COLS];
  va_list args;
  ssize_t res;
  int len;

  va_start(args, fmt);
  len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  res = write(STDOUT_FILENO, buf, len);
  (void)res;
}

static void tz_get_cursor(int *row, int *col) {
  char buf[32];
  int len;

  tz_write("\x1b[6n");

  len = tz_read(buf, sizeof(buf));

  if (len <= 0) {
    *row = 0;
    *col = 0;
    return;
  }

  sscanf(buf, "\x1b[%d;%dR", row, col);
}

static void tz_get_bounds(int *rows, int *cols) {
  int row[2];
  int col[2];

  /* save initial cursor position */
  tz_get_cursor(&row[0], &col[0]);

  /* move the cursor really far and query how far it actually moved */
  tz_write("\x1b[9999;9999H");
  tz_get_cursor(&row[1], &col[1]);

  /* restore initial cursor position */
  tz_write("\x1b[%d;%dH", row[0], col[0]);

  *rows = row[1];
  *cols = col[1];
}

static void tz_set_dirty(int x, int y) {
  uint64_t dirty_bit = UINT64_C(1) << (x & 63);
  tz.dirty[y >> 1][x / 64] |= dirty_bit;
}

static void tz_flush_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t depth) {
  /* assume pixel is dirty */
  tz_set_dirty(x, y);

  tz.color[y][x] = tz_color(r, g, b);
  tz.depth[y][x] = depth;
  tz.chars[y >> 1][x] = 0;
}

static void tz_set_color(int x, int y, uint32_t color) {
  uint32_t *old_color = &tz.color[y][x];
  char *old_c = &tz.chars[y >> 1][x];

  if (*old_color != color || *old_c != 0) {
    tz_set_dirty(x, y);
  }

  *old_color = color;
  *old_c = 0;
}

static void tz_set_char(int x, int y, uint32_t fg_color, uint32_t bg_color, uint8_t c) {
  uint32_t *old_fg_color = &tz.color[y & ~1][x];
  uint32_t *old_bg_color = &tz.color[y | 1][x];
  char *old_c = &tz.chars[y >> 1][x];

  if (*old_fg_color != fg_color || *old_bg_color != bg_color || *old_c != c) {
    tz_set_dirty(x, y);
  }

  *old_fg_color = fg_color;
  *old_bg_color = bg_color;
  *old_c = c;
}

static void tz_reset() {
  /* reset cursor */
  tz_write("\x1b[%d;1H", 1 + tz.y + tz.rows);
  tz_write("\x1b[0m");
  tz_write("\x1b[?25h");

  /* reset tty */
  tcsetattr(0, TCSANOW, &tz.old_tty);
}

static void tz_sigint(int sig) {
  tz_reset();

  /* uninstall ourself and re-raise */
  sigaction(SIGINT, &tz.old_sa, NULL);
  raise(SIGINT);
}

int tz_prompt(int y, const char *prompt, char *out, int n) {
  y += tz.y0;

  /* draw prompt */
  int row = y >> 1;
  tz_write("\x1b[%d;1H", 1 + row);
  tz_write(prompt);

  /* read line */
  char buf[TZ_MAX_COLS];
  int done = 0;
  int len = 0;

  while (!done) {
    int res = tz_read(buf, sizeof(buf));

    if (!res) {
      break;
    }

    if (buf[0] == '\x1b') {
      /* FIXME line editing and history */
    } else if (buf[0] == '\x7F') {
      /* handle backspace */
      if (len) {
        tz_write("\b \b");
        len--;
      }
    } else {
      for (int i = 0; i < res; i++) {
        char c = buf[i];

        if (c == '\0' || c == '\r') {
          done = 1;
          break;
        }

        tz_write("%c", c);

        if (isprint(c) && len < n) {
          out[len++] = c;
        }
      }
    }
  }

  if (len < n) {
    out[len] = 0;
  }

  /* erase prompt */
  tz_write("\x1b[1K");

  return len;
}

int tz_read(char *out, int n) {
  int res = read(STDIN_FILENO, out, n);

  if (res <= 0) {
    return 0;
  }

  if (out[0] == '\x3') {
    raise(SIGINT);
    return 0;
  }

  return res;
}

int tz_can_read() {
  struct pollfd fds = {
      .fd = STDIN_FILENO,
      .events = POLLIN,
  };

  int ret = poll(&fds, 1, 0);

  return ret > 0;
}

void tz_paint() {
  int last_fg_color = -1;
  int last_bg_color = -1;
  int last_row = -1;
  int last_col = -1;

  /* emit "begin synchronized update" code */
  tz_write("\x1b[?2026");

  for (int row = 0; row < tz.rows; row++) {
    for (int col = 0; col < tz.cols; col += 64) {
      uint64_t dirty = tz.dirty[row][col / 64];

      if (!dirty) {
        continue;
      }

      tz.dirty[row][col / 64] = 0;

      while (dirty) {
        int dirty_bit = tz_ctz64(dirty);
        dirty &= ~(UINT64_C(1) << dirty_bit);
        col |= dirty_bit;

        uint32_t fg_color = tz.color[(row << 1) + 0][col];
        uint32_t bg_color = tz.color[(row << 1) + 1][col];

        if (last_row == -1 || last_col == -1) {
          tz_write("\x1b[%d;%dH", 1 + tz.y + row, 1 + col);
        } else {
          int dy = row - last_row;
          int dx = col - last_col;

          if (dx || dy) {
            tz_write("\x1b[%d;%dH", 1 + tz.y + row, 1 + col);
          } else if (dx > 0) {
            tz_write("\x1b[%dC", dx);
          } else if (dx < 0) {
            tz_write("\x1b[%dD", -dx);
          } else if (dy > 0) {
            tz_write("\x1b[%dB", dy);
          } else if (dy < 0) {
            tz_write("\x1b[%dA", -dy);
          }
        }

        if (fg_color != last_fg_color) {
          uint8_t r = tz_red(fg_color);
          uint8_t g = tz_green(fg_color);
          uint8_t b = tz_blue(fg_color);
          tz_write("\x1b[38;2;%03d;%03d;%03dm", r, g, b);
        }

        if (bg_color != last_bg_color) {
          uint8_t r = tz_red(bg_color);
          uint8_t g = tz_green(bg_color);
          uint8_t b = tz_blue(bg_color);
          tz_write("\x1b[48;2;%03d;%03d;%03dm", r, g, b);
        }

        if (tz.chars[row][col]) {
          tz_write("%c", tz.chars[row][col]);
        } else {
          tz_write("%lc", 0x2580);
        }

        last_fg_color = fg_color;
        last_bg_color = bg_color;
        last_col = col + 1;
        last_row = row;

        /* reset col offset for next iteration */
        col &= ~63;
      }
    }
  }

  /* reset mode */
  tz_write("\x1b[0m");

  /* emit "end synchronized update" code */
  tz_write("\x1b[?2026l");

  /* check for ctrl-c after painting is done */
  char buf[TZ_MAX_COLS];

  while (tz_can_read()) {
    tz_read(buf, sizeof(buf));
  }
}

static int tz_skip_primitive(const struct tz_vertex *v0, const struct tz_vertex *v1, const struct tz_vertex *v2) {
  const struct tz_vertex *prim[] = {v0, v1, v2};
  int outside_viewport = 1;

  for (int i = 0; i < 3; i++) {
    const struct tz_vertex *v = prim[i];
    int outside_xy = v->x < -v->w || v->x > v->w || v->y < -v->w || v->y > v->w;
    int outside_z = v->z < 0.0f || v->z > v->w;

    outside_viewport &= outside_xy;

    /* ignore primitives with any vertex outside of the near / far plane */
    if (outside_z) {
      return 1;
    }
  }

  /* ignore primitives completely outside of the viewport */
  return outside_viewport;
}

void tz_triangle(const struct tz_vertex *v0, const struct tz_vertex *v1, const struct tz_vertex *v2) {
  if (tz_skip_primitive(v0, v1, v2)) {
    return;
  }

  /* translate to ndc space */
  float x0_norm = v0->x / v0->w;
  float y0_norm = v0->y / v0->w;
  float z0_norm = v0->z / v0->w;
  float x1_norm = v1->x / v1->w;
  float y1_norm = v1->y / v1->w;
  float z1_norm = v1->z / v1->w;
  float x2_norm = v2->x / v2->w;
  float y2_norm = v2->y / v2->w;
  float z2_norm = v2->z / v2->w;

  /* translate to screen space */
  int half_w = (tz.x1 - tz.x0 + 1) >> 1;
  int half_h = (tz.y1 - tz.y0 + 1) >> 1;
  int mid_x = tz.x0 + half_w;
  int mid_y = tz.y0 + half_h;

  float x0_screen = x0_norm * half_w;
  float y0_screen = y0_norm * -half_h;
  float x1_screen = x1_norm * half_w;
  float y1_screen = y1_norm * -half_h;
  float x2_screen = x2_norm * half_w;
  float y2_screen = y2_norm * -half_h;

  /* calculate the adjoint matrix

    FIXME ignore primitives that are going to overflow */
  int x0 = (int)(x0_screen * TZ_SUBPIXEL_STEP - 0.5f);
  int y0 = (int)(y0_screen * TZ_SUBPIXEL_STEP - 0.5f);
  int x1 = (int)(x1_screen * TZ_SUBPIXEL_STEP - 0.5f);
  int y1 = (int)(y1_screen * TZ_SUBPIXEL_STEP - 0.5f);
  int x2 = (int)(x2_screen * TZ_SUBPIXEL_STEP - 0.5f);
  int y2 = (int)(y2_screen * TZ_SUBPIXEL_STEP - 0.5f);

  int a0 = y1 - y2;
  int b0 = x2 - x1;
  int c0 = (int)(((int64_t)x1 * y2 - y1 * x2) >> TZ_SUBPIXEL_BITS);
  int a1 = y2 - y0;
  int b1 = x0 - x2;
  int c1 = (int)(((int64_t)x2 * y0 - y2 * x0) >> TZ_SUBPIXEL_BITS);
  int a2 = y0 - y1;
  int b2 = x1 - x0;
  int c2 = (int)(((int64_t)x0 * y1 - y0 * x1) >> TZ_SUBPIXEL_BITS);

  /* cull back faces */
  int area = c0 + c1 + c2;

  if (area <= 0) {
    return;
  }

  /* calculate bounding box for the primitive */
  int min_x = TZ_MIN(x0, TZ_MIN(x1, x2)) >> TZ_SUBPIXEL_BITS;
  int min_y = TZ_MIN(y0, TZ_MIN(y1, y2)) >> TZ_SUBPIXEL_BITS;
  int max_x = TZ_MAX(x0, TZ_MAX(x1, x2)) >> TZ_SUBPIXEL_BITS;
  int max_y = TZ_MAX(y0, TZ_MAX(y1, y2)) >> TZ_SUBPIXEL_BITS;

  min_x = TZ_MAX(min_x, -half_w);
  min_y = TZ_MAX(min_y, -half_h);
  max_x = TZ_MIN(max_x, half_w - 1);
  max_y = TZ_MIN(max_y, half_h - 1);

  /* rasterize immediately since there's no transparency */
  int w0_row = a0 * min_x + b0 * min_y + c0;
  int w1_row = a1 * min_x + b1 * min_y + c1;
  int w2_row = a2 * min_x + b2 * min_y + c2;

  for (int i = min_y; i <= max_y; i++) {
    int w0 = w0_row;
    int w1 = w1_row;
    int w2 = w2_row;

    for (int j = min_x; j <= max_x; j++) {
      /* check coverage */
      if ((w0 | w1 | w2) >= 0) {
        int x = mid_x + j;
        int y = mid_y + i;

        float z = z0_norm * w0 + z1_norm * w1 + z2_norm * w2;
        uint8_t depth = tz_clamp_u8((int)((z / area) * 0xff));

        /* check depth */
        if (depth < tz.depth[y][x]) {
          uint8_t r = tz_clamp_u8((int)((v0->r * w0 + v1->r * w1 + v2->r * w2) / z));
          uint8_t g = tz_clamp_u8((int)((v0->g * w0 + v1->g * w1 + v2->g * w2) / z));
          uint8_t b = tz_clamp_u8((int)((v0->b * w0 + v1->b * w1 + v2->b * w2) / z));

          tz_flush_pixel(x, y, r, g, b, depth);
        }
      }

      w0 += a0;
      w1 += a1;
      w2 += a2;
    }

    w0_row += b0;
    w1_row += b1;
    w2_row += b2;
  }
}

void tz_line(const struct tz_vertex *v0, const struct tz_vertex *v1) {
  if (tz_skip_primitive(v0, v1, v1)) {
    return;
  }

  /* translate to ndc space */
  float x0_norm = v0->x / v0->w;
  float y0_norm = v0->y / v0->w;
  float z0_norm = v0->z / v0->w;
  float x1_norm = v1->x / v1->w;
  float y1_norm = v1->y / v1->w;
  float z1_norm = v1->z / v1->w;

  /* translate to screen space */
  int half_w = (tz.x1 - tz.x0 + 1) >> 1;
  int half_h = (tz.y1 - tz.y0 + 1) >> 1;
  int mid_x = tz.x0 + half_w;
  int mid_y = tz.y0 + half_h;

  int x0 = (int)(x0_norm * half_w + mid_x);
  int y0 = (int)(y0_norm * -half_h + mid_y);
  int x1 = (int)(x1_norm * half_w + mid_x);
  int y1 = (int)(y1_norm * -half_h + mid_y);

  /* rasterize immediately */
  int dx = abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int e = dx + dy;
  int x = x0;
  int y = y0;

  float len = sqrt(dx * dx + dy * dy);
  float sf = 1.0f / len;
  float f = 0.0f;

  while (1) {
    float w0 = f;
    float w1 = 1.0f - f;

    float z = z0_norm * w0 + z1_norm * w1;
    uint8_t depth = tz_clamp_u8((int)(z * 0xff));

    /* check depth */
    if (depth < tz.depth[y][x]) {
      uint8_t r = tz_clamp_u8((int)((v0->r * w0 + v1->r * w1) / z));
      uint8_t g = tz_clamp_u8((int)((v0->g * w0 + v1->g * w1) / z));
      uint8_t b = tz_clamp_u8((int)((v0->b * w0 + v1->b * w1) / z));

      tz_flush_pixel(x, y, r, g, b, depth);
    }

    if (x == x1 && y == y1) {
      break;
    }

    int e2 = e * 2;

    if (e2 >= dy) {
      e += dy;
      x += sx;
      f += sf;
    }

    if (e2 <= dx) {
      e += dx;
      y += sy;
      f += sf;
    }
  }
}

void tz_blit(int x, int y, int w, int h, const uint32_t *data) {
  int x0 = tz.x0 + x;
  int y0 = tz.y0 + y;
  int x1 = TZ_MIN(x0 + w - 1, tz.x1);
  int y1 = TZ_MIN(y0 + h - 1, tz.y1);

  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      tz_set_color(x, y, *(data++));
    }
  }
}

int tz_print(int x, int y, const char *fmt, ...) {
  x += tz.x0;
  y += tz.y0;

  /* format the message */
  char buf[TZ_MAX_COLS * 8];
  va_list args;
  int n;

  va_start(args, fmt);
  n = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  /* parse escape codes while printing each character */
  int state = 0;
  char cmd = 0;
  int arg = 0;

  for (int i = 0, state = 0, cmd = 0; i < n; i++) {
    if (y < tz.y0 || y > tz.y1 || x > tz.x1) {
      break;
    }

    char c = buf[i];

    switch (state) {
      case 1: {
        if (c == '[') {
          state = 2;
        }
      } break;

      case 2: {
        state = 3;
        cmd = c;
      } break;

      case 3: {
        if (c == ';' || c == ']') {
          if (cmd == 'f') {
            tz.fg_color = ansi_lut[arg];
          } else {
            tz.bg_color = ansi_lut[arg];
          }

          arg = 0;

          if (c == ';') {
            state = 2;
          } else {
            state = 0;
          }
        } else {
          arg *= 10;
          arg += c - '0';
        }
      } break;

      default: {
        if (c == '\x1b') {
          state = 1;
        } else {
          if (x >= tz.x0) {
            tz_set_char(x, y, tz.fg_color, tz.bg_color, c);
          }

          x += 1;
        }
      } break;
    }
  }

  return x - tz.x0;
}

void tz_clear() {
  static const uint32_t clear_buffer[TZ_MAX_ROWS << 1][TZ_MAX_COLS];

  tz_blit(0, 0, tz.x1 - tz.x0 + 1, tz.y1 - tz.y0 + 1, (const uint32_t *)clear_buffer);

  memset(tz.depth, 0xff, sizeof(tz.depth));
}

void tz_viewport(int x, int y, int w, int h) {
  tz.x0 = TZ_MAX(x, 0);
  tz.y0 = TZ_MAX(y, 0);
  tz.x1 = TZ_MIN(x + w - 1, tz.cols - 1);
  tz.y1 = TZ_MIN(y + h - 1, (tz.rows << 1) - 1);
}

int tz_height() {
  return tz.rows << 1;
}

int tz_width() {
  return tz.cols;
}

void tz_init(int w, int h) {
  /* backup tty attributes */
  tcgetattr(0, &tz.old_tty);

  /* install SIGINT handler */
  struct sigaction new_sa;
  sigemptyset(&new_sa.sa_mask);
  new_sa.sa_handler = tz_sigint;
  new_sa.sa_flags = 0;
  sigaction(SIGINT, &new_sa, &tz.old_sa);

  /* install exit handler */
  atexit(tz_reset);

  /* setup raw tty */
  struct termios new_attrs;

  memcpy(&new_attrs, &tz.old_tty, sizeof(new_attrs));
  cfmakeraw(&new_attrs);

  tcsetattr(0, TCSANOW, &new_attrs);

  /* hide the cursor */
  tz_write("\x1b[?25l");

  /* enable utf-8 support */
  setlocale(LC_ALL, "en_US.UTF-8");

  /* determine canvas bounds */
  if (w && h) {
    tz.rows = h >> 1;
    tz.cols = w;
  } else {
    tz_get_bounds(&tz.rows, &tz.cols);
  }

  /* make room for the canvas and work out where the top is */
  int row, col;

  for (int i = 0; i < tz.rows - 1; i++) {
    tz_write("\n");
  }

  tz_get_cursor(&row, &col);

  tz.y = row - tz.rows;

  /* default viewport */
  tz.x0 = 0;
  tz.y0 = 0;
  tz.x1 = tz.cols - 1;
  tz.y1 = (tz.rows << 1) - 1;

  /* set sane default colors */
  tz.fg_color = tz_color(0xff, 0xff, 0xff);
  tz.bg_color = tz_color(0x00, 0x00, 0x00);

  /* mark all cells dirty for the first paint */
  for (int row = 0; row < tz.rows; row++) {
    for (int col = 0; col < tz.cols; col += 64) {
      int bits = TZ_MIN(tz.cols - col, 64);

      tz.dirty[row][col / 64] = UINT64_C(-1) >> (64 - bits);
    }
  }
}

#endif
