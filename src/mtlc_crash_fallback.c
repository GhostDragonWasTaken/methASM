#ifdef _WIN32
#include "runtime/crash_handler.h"

const char *mettle_crash_exception_name(DWORD code) {
  (void)code;
  return "exception";
}
#endif
