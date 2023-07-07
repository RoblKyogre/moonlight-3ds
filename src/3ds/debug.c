#ifdef DEBUG
#include <sys/iosupport.h>
#include <3ds/3dslink.h>

#include <stdlib.h>
#include <stdio.h>

static ssize_t n3ds_log_write(struct _reent* r, void* fd, const char* ptr, size_t len) {
  printf("%*.*s", len, len, ptr);
  return len;
}
static const devoptab_t dotab_stdout = {
  .name = "stdout",
  .write_r = n3ds_log_write,
};

void __wrap_abort() {
  printf("Abort called! Forcing stack trace.");
  *(unsigned int*)(0xDEADC0DE) = 0xBABECAFE;
  for(;;) {}
}

void Debug_Init() {
  int res = link3dsStdio();
  printf("%d",res);
  //devoptab_list[STD_OUT] = &dotab_stdout;
  //devoptab_list[STD_ERR] = &dotab_stdout;
}
#endif
