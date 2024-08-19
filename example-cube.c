#include <sys/time.h>

#define QUICKMATHS_IMPLEMENTATION
#include "quickmaths.h"

#define TERMINIZER_IMPLEMENTATION
#include "terminizer.h"

#define MAX_SAMPLES 100

static int64_t gettime_ms() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (((int64_t)tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}

int main() {
  tz_init(128, 72);

  /* scene state */
  const int canvas_width = tz_width();
  const int canvas_height = tz_height();

  const vec3_t camera_origin = {
      0.0f,
      0.0f,
      0.0f,
  };
  const vec3_t camera_axes[3] = {
      {1.0f, 0.0f, 0.0f},
      {0.0f, 1.0f, 0.0f},
      {0.0f, 0.0f, 1.0f},
  };
  const float znear = 1.0f;
  const float zfar = 16384.0f;

  mat4_t modelview_matrix;
  mat4_t projection_matrix;
  mat4_t mvp_matrix;

  int64_t time_samples[MAX_SAMPLES] = {};
  int64_t time_sum = 0;
  int64_t time_seq = 0;

  mat4_camera(modelview_matrix, camera_origin, camera_axes);
  mat4_perspective(projection_matrix, 90.0f, canvas_width, canvas_height, znear, zfar);
  mat4_mul(mvp_matrix, projection_matrix, modelview_matrix);

  /* cube state

        5--------4
       /|       /|
      / |      / |
     0--------1  |
     |  |     |  |
     |  7-----|--6
     | /      | /
     |/       |/
     2--------3 */
  const struct tz_vertex cube_verts[8] = {
      {-1.0f, +1.0f, -1.0f, +1.0f, 0xFF, 0x00, 0x00},
      {+1.0f, +1.0f, -1.0f, +1.0f, 0xFF, 0xFF, 0x00},
      {-1.0f, -1.0f, -1.0f, +1.0f, 0x00, 0xFF, 0x00},
      {+1.0f, -1.0f, -1.0f, +1.0f, 0x00, 0x00, 0xFF},

      {+1.0f, +1.0f, +1.0f, +1.0f, 0xFF, 0x00, 0x00},
      {-1.0f, +1.0f, +1.0f, +1.0f, 0xFF, 0xFF, 0x00},
      {+1.0f, -1.0f, +1.0f, +1.0f, 0x00, 0xFF, 0x00},
      {-1.0f, -1.0f, +1.0f, +1.0f, 0x00, 0x00, 0xFF},
  };
  const int cube_faces[][4] = {
      {0, 1, 2, 3}, /* front */
      {1, 4, 3, 6}, /* right */
      {4, 5, 6, 7}, /* back */
      {5, 0, 7, 2}, /* left */
      {5, 4, 0, 1}, /* top */
      {2, 3, 7, 6}, /* bot */
  };
  vec3_t cube_origin = {0.0f, 0.0f, 3.5f};
  float cube_pitch = 0.0f;
  float cube_yaw = 0.0f;

  while (1) {
    int64_t time_begin = gettime_ms();

    tz_clear();

    /* transform verts */
    struct tz_vertex verts[8];
    mat4_t cube_rotate[3];

    memcpy(verts, cube_verts, sizeof(verts));

    mat4_rotate_pitch(cube_rotate[0], -cube_pitch);
    mat4_rotate_yaw(cube_rotate[1], -cube_yaw);

    mat4_mul(cube_rotate[2], cube_rotate[0], cube_rotate[1]);

    for (int i = 0; i < sizeof(verts) / sizeof(verts[0]); i++) {
      struct tz_vertex *v = &verts[i];

      /* rotate the cube */
      mat4_transform(v->pos, cube_rotate[2], v->pos);
      vec3_add(v->pos, v->pos, cube_origin);

      /* translate to clip space */
      mat4_transform(v->pos, mvp_matrix, v->pos);
    }

    /* draw faces */
    for (int i = 0; i < sizeof(cube_faces) / sizeof(cube_faces[0]); i++) {
      struct tz_vertex *v0 = &verts[cube_faces[i][0]];
      struct tz_vertex *v1 = &verts[cube_faces[i][1]];
      struct tz_vertex *v2 = &verts[cube_faces[i][2]];
      struct tz_vertex *v3 = &verts[cube_faces[i][3]];

      tz_triangle(v0, v1, v2);
      tz_triangle(v2, v1, v3);
    }

    /* draw frame rate */
    int fps = (int)(1000.0 / ((double)time_sum / MAX_SAMPLES));
    tz_print(canvas_width - 8, 0, "\x1b[f15]%4d FPS", fps);

    /* repaint */
    tz_paint();

    /* update simple moving average */
    int64_t time_end = gettime_ms();
    int64_t time_elapsed = time_end - time_begin;

    time_sum -= time_samples[time_seq];
    time_sum += time_elapsed;
    time_samples[time_seq] = time_elapsed;
    time_seq = (time_seq + 1) % MAX_SAMPLES;

    /* update rotation */
    float delta_time = time_elapsed / 1000.0f;
    cube_pitch += 3.0f * delta_time;
    cube_yaw += 5.0f * delta_time;
  }

  return 0;
}
