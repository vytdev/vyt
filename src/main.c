#include "vyt.h"
#include "utils.h"
#include "exec.h"
#include "mem.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdatomic.h>

// for checking whether stdin is redirected
#if defined(_WIN32) || defined(_WIN64)
#   include <io.h>
#   define isatty _isatty
#   define fileno _fileno
#else
#   include <unistd.h>
#endif

// print help message
void print_help(char *prog);

// print a byte array
void print_data(char *data, int size, char cols);

int main(int argc, char **argv) {

  // arguments
  char    arg_help  = 0;
  vqword  arg_stack = 1048576; // default: 1 MiB

  // source file
  char srcset    = 0;
  char use_stdin = 0;
  char use_file  = 0;

  // file name
  char *name = "-";

#ifndef ARGERR
#define ARGERR(msg, ...)                 \
  fprintf(stderr,                        \
    "%s: " msg                           \
    "try '%s --help' for more info.\n",  \
    argv[0], __VA_ARGS__, argv[0]        \
  )
#endif // ARGERR

  // our argument parser :)
  int i = 1;
  while (i < argc) {
    char *arg = argv[i];
    int len = strlen(arg);

    // not an option
    if (arg[0] != '-') {
      if (!srcset) {
        name = arg;
        srcset = 1;
        use_file = 1;
        i++;
      }
      break;
    }

    // option delimeter (--)
    if (len == 2 && arg[1] == '-') {
      i++;
      arg = argv[i];
      if (!srcset && arg) {
        name = arg;
        srcset = 1;
        use_file = 1;
        i++;
      }
      break;
    }

    // read from stdin
    if (len == 1) {
      if (!srcset) {
        srcset = 1;
        use_stdin = 1;
      }
      i++;
      continue;
    }

    // stack size option
    if (arg[1] == 't') {
      char *num = arg + 2;
      // -t=123
      if (arg[2] == '=') {
        num++;
      }
      // -t 123 OR -t= 123*/
      if (num[0] == '\0') {
        i++;
        // too few arguments
        if (i >= argc) {
          ARGERR("%s: required option argument not specified\n", arg);
          return 1;
        }
        // use the next parameter as the argument
        num = argv[i];
      }
      // check if string is a number
      int validnum = 1;
      char *scanner = num;
      while (*scanner != '\0') {
        if (*scanner < '0' || *scanner > '9') {
          validnum = 0;
          break;
        }
        scanner++;
      }
      // invalid number
      if (!validnum) {
        ARGERR("%s: invalid number\n", num);
        return 1;
      }
      // parse the number
      arg_stack = strtoull(num, NULL, 10);
      i++;
      continue;
    }

    // short flags (-f)
    if (arg[1] != '-') {
      for (int c = 1; c < len; c++) {
        switch (arg[c]) {
          case 'h': arg_help = 1; break;
          case 't':
            ARGERR(
              "-%c: cannot use this independent option as a flag\n",
              arg[c]
            );
            return 1;
          // unknown flag
          default:
            ARGERR("-%c: unknown flag\n", arg[c]);
            return 1;
        }
      }
      i++;
      continue;
    }

    // long flags (--flag)
    if      (strcmp(arg + 2, "help") == 0) { arg_help = 1; }
    // unknown flag
    else {
      ARGERR("%s: unknown flag\n", arg);
      return 1;
    }
    i++;
  }
  // default source
  if (!srcset && !isatty(fileno(stdin))) {
    srcset = 1;
    use_stdin = 1;
  }

#ifdef ARGERR
#undef ARGERR
#endif // ARGERR

  // help message
  if (arg_help) {
    print_help(argv[0]);
    return 1;
  }

  // no arguments passed
  if (!srcset) {
    fprintf(stderr, "%s: no file specified\n", argv[0]);
    print_help(argv[0]);
    return 1;
  }

  // the source stream
  FILE *src = NULL;

  // read from standard input
  if (use_stdin) {
    if (isatty(fileno(stdin))) {
      fprintf(stderr, "%s: cannot read stdin\n", argv[0]);
      return 1;
    }
    src = stdin;
  }
  // read from a file
  else if (use_file) {
    src = fopen(name, "rb");
    if (src == NULL) {
      fprintf(stderr, "%s: error opening file '%s'\n", argv[0], name);
      return 1;
    }
  }

  // allocate memory on buffer
  vbyte *buffer  = (vbyte*)malloc(4096);
  size_t bufalloc = 4096;
  size_t bufsize  = 0;

  // allocation failed
  if (!buffer) {
    fprintf(stderr, "%s: error allocating memory for buffer\n", argv[0]);
    return 1;
  }

  // buffered reading
  while (1) {
    size_t bytes_in = fread(buffer + bufsize, 1, 4096, src);
    bufsize += bytes_in;

    // end of file
    if (feof(src)) {
      break;
    }

    // nothing was read
    if (bytes_in == 0) {
      fprintf(stderr, "%s: error reading input stream\n", argv[0]);
      free(buffer);
      return 1;
    }

    // resize the buffer when needed
    if (bufalloc - bufsize < 4096) {
      bufsize *= 2;
      vbyte *tmp = (vbyte*)realloc(buffer, bufsize);
      if (tmp == NULL) {
        fprintf(stderr, "%s: error resizing the buffer\n", argv[0]);
        free(buffer);
        return 1;
      }
      buffer = tmp;
    }
  }

  // close the file stream
  if (use_file) {
    fclose(src);
  }

  // our startup options
  struct vopts opt = {
    .stacksz = arg_stack,
  };

  vproc p;
  int stat = 0;

  // initialize the vm
  stat = vpinit(&p, &opt);
  if (VOK != stat) {
    fprintf(stderr, "%s: failed to initialize vm: ", argv[0]);
    vperr(stat);
    free(buffer);
    return stat;
  }
  stat = vload(&p, buffer, bufsize);
  if (VOK != stat) {
    fprintf(stderr, "%s: failed to load program: ", argv[0]);
    vperr(stat);
    vpdestroy(&p);
    free(buffer);
    return stat;
  }
  free(buffer);

  // prints memory mappings (uncomment if needed)
  // for (vqword n = 0; n < p._page_alloc; n++) {
  //   if (p.page[n].flags & VFUSED) {
  //     printf("PAGE %llx:\n", p.page[n].ndx);
  //     if (NULL == p.page[n].frame) {
  //       printf("(uninitialized)\n");
  //     }
  //     else {
  //       print_data((char*)p.page[n].frame, VPAGESZ, 16);
  //     }
  //     printf("\n");
  //   }
  // }

  // execute the program
  stat = vrun(&p); // name, &argv[i], argc - i);
  if (VOK != stat) {
    fprintf(stderr, "%s: aborting due to critical error: ", argv[0]);
    vperr(stat);
    vpdestroy(&p);
    return stat;
  }

  // free resources
  int extc = atomic_load(&p.exitcode);
  vpdestroy(&p);
  return extc;
}

void print_help(char *prog) {
	fprintf(stderr,
		"usage: %s [options ...] [file | -] [args ...]\n"
		"\n"
		"options:\n"
		"    --             indicates the end of options\n"
		"    -              read file from stdin\n"
		"    -h, --help     show this help and exit\n"
		"    -t size        set the stack size\n"
		"\n"
		"arguments:\n"
		"    file           input file name\n"
		"    args           arguments to pass to the program\n"
		"\n"
		"vyt runtime engine (abi version %d)\n",
		prog, VVERSION
	);
}

void print_data(char *data, int size, char cols) {
  for (int i = 0; i < size; i += cols) {
    printf("%08x:   ", i);

    for (int j = i; j < i + cols; j++) {
      if (j >= size) {
        printf("   ");
        continue;
      }

      if (
        ' ' == data[j] || '\t' == data[j] || '\v' == data[j] ||
        '\n' == data[j] || '\f' == data[j] || '\r' == data[j]
      ) { printf("\033[33m"); }
      else if (0xff == data[j]) { printf("\033[34m"); }
      else if (0 == data[j]) { printf("\033[0m"); }
      else if (0x20 > data[j] || 0x7e < data[j]) { printf("\033[31m"); }
      else { printf("\033[32m"); }

      printf("%02X ", data[j]);
    }

    printf("\033[0m  |");

    for (int j = i; j < i + cols; j++) {
      if (j >= size) {
        break;
      }

      if (
        ' ' == data[j] || '\t' == data[j] || '\v' == data[j] ||
        '\n' == data[j] || '\f' == data[j] || '\r' == data[j]
      ) { printf("\033[33m"); }
      else if (0xff == data[j]) { printf("\033[34m"); }
      else if (0 == data[j]) { printf("\033[0m"); }
      else if (0x20 > data[j] || 0x7e < data[j]) { printf("\033[31m"); }
      else { printf("\033[32m"); }

      if (0x20 > data[j] || 0x7e < data[j]) {
        putchar('.');
        continue;
      }
      putchar(data[j]);
    }

    printf("\033[0m|\n");
  }
}
