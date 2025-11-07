#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef MIN
#define MIN(x, y)   (x > y)?(y):(x)
#endif

#ifndef MAX
#define MAX(x, y)   (x > y)?(x):(y)
#endif

#ifndef NELEM
#define NELEM(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))
#endif /* NELEM */

#ifndef NULL
#define NULL      (0)
#endif /* NULL */

#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN  (4096)
#endif /* MAX_PATH_LEN */

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif /* UNUSED */

#ifndef UNUSED_PARAM
#define UNUSED_PARAM __attribute__((unused))
#endif /* UNUSED_PARAM */

#ifndef SAFE_FREE
#define SAFE_FREE(x) if (x != NULL) { free(x); x = NULL; }
#endif /* SAFE_FREE */

#ifndef SAFE_DELETE
#define SAFE_DELETE(x) if (x != NULL) { delete x; x = NULL; }
#endif /* SAFE_DELETE */

#ifndef SAFE_CLOSE
#define SAFE_CLOSE(fd) if (fd > 0) { ::close(fd); fd = -1; }
#endif /* SAFE_CLOSE */

// Put this in the private: declarations for a class to be uncopyable.
#define DISALLOW_COPY(TypeName) \
  TypeName(const TypeName&)

// Put this in the private: declarations for a class to be unassignable.
#define DISALLOW_ASSIGN(TypeName) \
  void operator=(const TypeName&)

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)
