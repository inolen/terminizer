#ifndef QUICKMATHS_H
#define QUICKMATHS_H

#include <float.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define DEG2RAD(x) ((x) * (M_PI / 180.f))

enum {
  PITCH = 0,
  YAW = 1,
  ROLL = 2,
};

typedef float mat4_t[16];
typedef float vec3_t[3];
typedef float vec4_t[4];

void mat4_ident(mat4_t out);
void mat4_mul(mat4_t out, const mat4_t a, const mat4_t b);
void mat4_transform(vec4_t out, const mat4_t m, const vec4_t a);

void mat4_camera(mat4_t out, const vec3_t origin, const vec3_t axes[3]);
void mat4_perspective(mat4_t out, float fov_y, int w, int h, float near, float far);

void mat4_rotate_pitch(mat4_t out, float rads);
void mat4_rotate_yaw(mat4_t out, float rads);
void mat4_rotate_roll(mat4_t out, float rads);

void vec3_copy(vec3_t out, const vec3_t a);
void vec3_add(vec3_t out, const vec3_t a, const vec3_t b);
void vec3_sub(vec3_t out, const vec3_t a, const vec3_t b);
float vec3_dot(const vec3_t a, const vec3_t b);
float vec3_len(const vec3_t a);
float vec3_normalize(vec3_t a);

void vec4_copy(vec4_t out, const vec4_t a);

#endif

#ifdef QUICKMATHS_IMPLEMENTATION

void vec4_copy(vec4_t out, const vec4_t a) {
  out[0] = a[0];
  out[1] = a[1];
  out[2] = a[2];
  out[3] = a[3];
}

float vec3_normalize(vec3_t a) {
  float len = vec3_len(a);

  if (len) {
    a[0] /= len;
    a[1] /= len;
    a[2] /= len;
  }

  return len;
}

float vec3_len(const vec3_t a) {
  return sqrtf(vec3_dot(a, a));
}

float vec3_dot(const vec3_t a, const vec3_t b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void vec3_sub(vec3_t out, const vec3_t a, const vec3_t b) {
  out[0] = a[0] - b[0];
  out[1] = a[1] - b[1];
  out[2] = a[2] - b[2];
}

void vec3_add(vec3_t out, const vec3_t a, const vec3_t b) {
  out[0] = a[0] + b[0];
  out[1] = a[1] + b[1];
  out[2] = a[2] + b[2];
}

void vec3_copy(vec3_t out, const vec3_t a) {
  out[0] = a[0];
  out[1] = a[1];
  out[2] = a[2];
}

void mat4_rotate_roll(mat4_t out, float rads) {
  float cr = cosf(rads);
  float sr = sinf(rads);

  out[0] = cr;
  out[4] = -sr;
  out[8] = 0.0f;
  out[12] = 0.0f;

  out[1] = sr;
  out[5] = cr;
  out[9] = 0.0f;
  out[13] = 0.0f;

  out[2] = 0.0f;
  out[6] = 0.0f;
  out[10] = 1.0f;
  out[14] = 0.0f;

  out[3] = 0.0f;
  out[7] = 0.0f;
  out[11] = 0.0f;
  out[15] = 1.0f;
}

void mat4_rotate_yaw(mat4_t out, float rads) {
  float cy = cosf(rads);
  float sy = sinf(rads);

  out[0] = cy;
  out[4] = 0.0f;
  out[8] = sy;
  out[12] = 0.0f;

  out[1] = 0.0f;
  out[5] = 1.0f;
  out[9] = 0.0f;
  out[13] = 0.0f;

  out[2] = -sy;
  out[6] = 0.0f;
  out[10] = cy;
  out[14] = 0.0f;

  out[3] = 0.0f;
  out[7] = 0.0f;
  out[11] = 0.0f;
  out[15] = 1.0f;
}

void mat4_rotate_pitch(mat4_t out, float rads) {
  float cp = cosf(rads);
  float sp = sinf(rads);

  out[0] = 1.0f;
  out[4] = 0.0f;
  out[8] = 0.0f;
  out[12] = 0.0f;

  out[1] = 0.0f;
  out[5] = cp;
  out[9] = -sp;
  out[13] = 0.0f;

  out[2] = 0.0f;
  out[6] = sp;
  out[10] = cp;
  out[14] = 0.0f;

  out[3] = 0.0f;
  out[7] = 0.0f;
  out[11] = 0.0f;
  out[15] = 1.0f;
}

void mat4_perspective(mat4_t out, float fov_y, int w, int h, float near, float far) {
  float screen_aspect = (float)w / h;

  float frustum_height = near * tanf(fov_y * M_PI / 360.0f);
  float frustum_width = frustum_height * screen_aspect;
  float q = far / (far - near);

  out[0] = near / frustum_width;
  out[4] = 0.0f;
  out[8] = 0.0f;
  out[12] = 0.0f;

  out[1] = 0.0f;
  out[5] = near / frustum_height;
  out[9] = 0.0f;
  out[13] = 0.0f;

  out[2] = 0.0f;
  out[6] = 0.0f;
  out[10] = q;
  out[14] = -q * near;

  out[3] = 0.0f;
  out[7] = 0.0f;
  out[11] = 1.0f;
  out[15] = 0.0f;
}

void mat4_camera(mat4_t out, const vec3_t origin, const vec3_t axes[3]) {
  out[0] = axes[0][0];
  out[4] = axes[0][1];
  out[8] = axes[0][2];
  out[12] = -vec3_dot(origin, axes[0]);

  out[1] = axes[1][0];
  out[5] = axes[1][1];
  out[9] = axes[1][2];
  out[13] = -vec3_dot(origin, axes[1]);

  out[2] = axes[2][0];
  out[6] = axes[2][1];
  out[10] = axes[2][2];
  out[14] = -vec3_dot(origin, axes[2]);

  out[3] = 0.0f;
  out[7] = 0.0f;
  out[11] = 0.0f;
  out[15] = 1.0f;
}

void mat4_transform(vec4_t out, const mat4_t m, const vec4_t a) {
  vec4_t tmp;

  vec4_copy(tmp, a);

  out[0] = m[0] * tmp[0] + m[4] * tmp[1] + m[8] * tmp[2] + m[12] * tmp[3];
  out[1] = m[1] * tmp[0] + m[5] * tmp[1] + m[9] * tmp[2] + m[13] * tmp[3];
  out[2] = m[2] * tmp[0] + m[6] * tmp[1] + m[10] * tmp[2] + m[14] * tmp[3];
  out[3] = m[3] * tmp[0] + m[7] * tmp[1] + m[11] * tmp[2] + m[15] * tmp[3];
}

void mat4_mul(mat4_t out, const mat4_t a, const mat4_t b) {
  out[0] = a[0] * b[0] + a[4] * b[1] + a[8] * b[2] + a[12] * b[3];
  out[4] = a[0] * b[4] + a[4] * b[5] + a[8] * b[6] + a[12] * b[7];
  out[8] = a[0] * b[8] + a[4] * b[9] + a[8] * b[10] + a[12] * b[11];
  out[12] = a[0] * b[12] + a[4] * b[13] + a[8] * b[14] + a[12] * b[15];

  out[1] = a[1] * b[0] + a[5] * b[1] + a[9] * b[2] + a[13] * b[3];
  out[5] = a[1] * b[4] + a[5] * b[5] + a[9] * b[6] + a[13] * b[7];
  out[9] = a[1] * b[8] + a[5] * b[9] + a[9] * b[10] + a[13] * b[11];
  out[13] = a[1] * b[12] + a[5] * b[13] + a[9] * b[14] + a[13] * b[15];

  out[2] = a[2] * b[0] + a[6] * b[1] + a[10] * b[2] + a[14] * b[3];
  out[6] = a[2] * b[4] + a[6] * b[5] + a[10] * b[6] + a[14] * b[7];
  out[10] = a[2] * b[8] + a[6] * b[9] + a[10] * b[10] + a[14] * b[11];
  out[14] = a[2] * b[12] + a[6] * b[13] + a[10] * b[14] + a[14] * b[15];

  out[3] = a[3] * b[0] + a[7] * b[1] + a[11] * b[2] + a[15] * b[3];
  out[7] = a[3] * b[4] + a[7] * b[5] + a[11] * b[6] + a[15] * b[7];
  out[11] = a[3] * b[8] + a[7] * b[9] + a[11] * b[10] + a[15] * b[11];
  out[15] = a[3] * b[12] + a[7] * b[13] + a[11] * b[14] + a[15] * b[15];
}

void mat4_ident(mat4_t out) {
  out[0] = 1.0f;
  out[4] = 0.0f;
  out[8] = 0.0f;
  out[12] = 0.0f;

  out[1] = 0.0f;
  out[5] = 1.0f;
  out[9] = 0.0f;
  out[13] = 0.0f;

  out[2] = 0.0f;
  out[6] = 0.0f;
  out[10] = 1.0f;
  out[14] = 0.0f;

  out[3] = 0.0f;
  out[7] = 0.0f;
  out[11] = 0.0f;
  out[15] = 1.0f;
}

#endif
