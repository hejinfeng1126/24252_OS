#include "types.h"
#include "user.h"
#include "date.h"

int
main(int argc, char *argv[])
{
  struct rtcdate r;

  if (date(&r)) {
    printf(2, "date failed\n");
    exit();
  }

  // 按照 yyyy-mm-dd HH:MM:SS 格式打印时间
  printf(1, "%d-%d-%d %d:%d:%d UTC\n", 
         r.year, r.month, r.day,
         r.hour, r.minute, r.second);

  exit();
} 