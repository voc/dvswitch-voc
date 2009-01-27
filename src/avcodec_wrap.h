// Copyright 2009 Ben Hutchings.
// See the file "COPYING" for licence details.

// Some versions of ffmpeg define an ABS macro, which glib also does.
// The two definitions are equivalent but the duplicate definitions
// provoke a warning.
#undef ABS

// These guards were removed from <avcodec.h>... what were they thinking?
#ifdef __cplusplus
extern "C" {
#endif

#include <avcodec.h>

#ifdef __cplusplus
}
#endif

#undef ABS
