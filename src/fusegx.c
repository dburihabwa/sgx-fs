#include <stdio.h>
#include <stdlib.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

int main(int argc, char** argv) {
  struct fuse_operations *operations;

  struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
  int i;
  for (i = 0; i < argc; i++) {
    fuse_opt_add_arg(&args, argv[i]);
  }
  fuse_opt_add_arg(&args, "-omodules=subdir,subdir=/");

  fuse_main(args.argc, args.argv, operations, NULL);
  return EXIT_SUCCESS;
}