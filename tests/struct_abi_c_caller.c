
#include <stdint.h>

typedef struct {
  int32_t a, b, c;
} ThreeI32;

typedef struct {
  double x, y;
} TwoF64;

typedef struct {
  int64_t i;
  double d;
} MixedI64F64;

typedef struct {
  int32_t a, b, c, d, e, f, g, h;
} Big32;

extern int32_t mettle_take_three(ThreeI32 t);
extern ThreeI32 mettle_make_three(int32_t a, int32_t b, int32_t c);
extern int32_t mettle_take_two_f64(TwoF64 v);
extern int32_t mettle_take_mixed(MixedI64F64 v);
extern int32_t mettle_take_big32(Big32 v);

int mettle_c_driver_check(void) {
  ThreeI32 t = {11, 22, 33};
  TwoF64 p = {10.5, 21.5};
  MixedI64F64 m = {100, 7.0};
  Big32 b = {1, 2, 3, 4, 5, 6, 7, 8};
  ThreeI32 made;
  int32_t total = 0;

  total += mettle_take_three(t);
  made = mettle_make_three(3, 4, 5);
  total += made.a + made.b + made.c;
  total += mettle_take_two_f64(p);
  total += mettle_take_mixed(m);
  total += mettle_take_big32(b);
  return (int)total;
}
