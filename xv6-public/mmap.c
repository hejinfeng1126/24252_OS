#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <math.h>

static size_t page_size;

// align_down - rounds a value down to an alignment
// @x: the value
// @a: the alignment (must be power of 2)
//
// Returns an aligned value.
#define align_down(x, a) ((x) & ~((typeof(x))(a) - 1))

#define AS_LIMIT	(1 << 25) // Maximum limit on virtual memory bytes
#define MAX_SQRTS	(1 << 27) // Maximum limit on sqrt table entries
static double *sqrts;

// 全局变量跟踪最后映射的页面
static void *last_mapped_page = NULL;

// Use this helper function as an oracle for square root values.
static void
calculate_sqrts(double *sqrt_pos, int start, int nr)
{
  int i;

  for (i = 0; i < nr; i++)
    sqrt_pos[i] = sqrt((double)(start + i));
}

// 重写信号处理函数，确保它能正确捕获和处理段错误
static void
handle_sigsegv(int sig, siginfo_t *si, void *ctx)
{
  /* 获取导致段错误的地址 */
  uintptr_t fault_addr = (uintptr_t)si->si_addr;
  
  /* 若要移除这条消息，看到oops就代表你的修改没生效 */
  // printf("Debug: handling SIGSEGV at 0x%lx\n", fault_addr); 
  
  /* 判断错误地址是否在sqrts数组范围内 */
  if (fault_addr < (uintptr_t)sqrts || 
      fault_addr >= (uintptr_t)sqrts + MAX_SQRTS * sizeof(double)) {
    fprintf(stderr, "Error: SIGSEGV outside sqrts array range\n");
    exit(EXIT_FAILURE);
  }
  
  /* 计算包含错误地址的页面起始地址 */
  void *page_addr = (void *)align_down(fault_addr, page_size);
  
  /* 计算页面对应的数组索引 */
  size_t offset = (uintptr_t)page_addr - (uintptr_t)sqrts;
  int start_idx = offset / sizeof(double);
  
  /* 计算页面可容纳的double值数量 */
  int values_per_page = page_size / sizeof(double);
  if (start_idx + values_per_page > MAX_SQRTS) {
    values_per_page = MAX_SQRTS - start_idx;
  }
  
  /* 在分配新内存前取消映射上一页面 */
  if (last_mapped_page != NULL && last_mapped_page != page_addr) {
    if (munmap(last_mapped_page, page_size) == -1) {
      perror("munmap failed");
      exit(EXIT_FAILURE);
    }
  }
  
  /* 映射新页面到错误地址 */
  void *mapped = mmap(page_addr, page_size, 
                      PROT_READ | PROT_WRITE,
                      MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, 
                      -1, 0);
  
  if (mapped == MAP_FAILED) {
    perror("mmap failed");
    exit(EXIT_FAILURE);
  }
  
  /* 计算页面中的平方根值 */
  calculate_sqrts((double *)mapped, start_idx, values_per_page);
  
  /* 保存当前页面地址 */
  last_mapped_page = mapped;
}

static void
setup_sqrt_region(void)
{
  struct rlimit lim = {AS_LIMIT, AS_LIMIT};
  struct sigaction act;

  // Only mapping to find a safe location for the table.
  sqrts = mmap(NULL, MAX_SQRTS * sizeof(double) + AS_LIMIT, PROT_NONE,
	       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (sqrts == MAP_FAILED) {
    fprintf(stderr, "Couldn't mmap() region for sqrt table; %s\n",
	    strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Now release the virtual memory to remain under the rlimit.
  if (munmap(sqrts, MAX_SQRTS * sizeof(double) + AS_LIMIT) == -1) {
    fprintf(stderr, "Couldn't munmap() region for sqrt table; %s\n",
            strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Set a soft rlimit on virtual address-space bytes.
  if (setrlimit(RLIMIT_AS, &lim) == -1) {
    fprintf(stderr, "Couldn't set rlimit on RLIMIT_AS; %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Register a signal handler to capture SIGSEGV.
  act.sa_sigaction = handle_sigsegv;
  act.sa_flags = SA_SIGINFO;
  sigemptyset(&act.sa_mask);
  if (sigaction(SIGSEGV, &act, NULL) == -1) {
    fprintf(stderr, "Couldn't set up SIGSEGV handler;, %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
}

static void
test_sqrt_region(void)
{
  int i, pos = rand() % (MAX_SQRTS - 1);
  double correct_sqrt;

  printf("Validating square root table contents...\n");
  srand(0xDEADBEEF);

  for (i = 0; i < 500000; i++) {
    if (i % 2 == 0)
      pos = rand() % (MAX_SQRTS - 1);
    else
      pos += 1;
    calculate_sqrts(&correct_sqrt, pos, 1);
    if (sqrts[pos] != correct_sqrt) {
      fprintf(stderr, "Square root is incorrect. Expected %f, got %f.\n",
              correct_sqrt, sqrts[pos]);
      exit(EXIT_FAILURE);
    }
  }

  printf("All tests passed!\n");
}

int
main(int argc, char *argv[])
{
  page_size = sysconf(_SC_PAGESIZE);
  printf("page_size is %ld\n", page_size);
  setup_sqrt_region();
  test_sqrt_region();
  return 0;
}
