#ifndef _VYT
#define _VYT
#include <stdint.h>

typedef uint8_t  vbyte;
typedef uint16_t vword;
typedef uint32_t vdword;
typedef uint64_t vqword;

/* return codes */
#define VOK         0
#define VERROR      (-1)
#define VENULL      (-2)
#define VENOMEM     (-3)
#define VESEGV      (-4)
#define VEACCES     (-5)

/**
 * print error by return code
 * returns the given return code
 */
int vperr(int ret);

#endif // _VYT
