.text

/* Switch from current_thread to next_thread. Make next_thread
 * the current_thread, and set next_thread to 0.
 * Use eax as a temporary register; it is caller saved.
 */
	.globl thread_switch
thread_switch:
	/* 保存当前线程状态 */
	pushal                      /* 保存所有寄存器到当前栈 */
	
	/* 获取当前线程和下一线程的指针 */
	movl current_thread, %eax   /* 获取当前线程指针 */
	movl %esp, (%eax)           /* 保存当前栈指针到current_thread->sp */
	
	/* 切换到下一个线程 */
	movl next_thread, %eax      /* 获取下一线程指针 */

	/* 检查next_thread是否为NULL */
	cmpl $0, %eax
	je thread_done              /* 如果是NULL, 跳转到结束 */
	
	movl %eax, current_thread   /* 将next_thread设为current_thread */
	movl $0, next_thread        /* 将next_thread设为0 */
	
	/* 从线程结构体加载栈指针 */
	movl (%eax), %esp           /* 加载next_thread->sp到栈指针 */
	
thread_done:
	popal                       /* 从新线程的栈上恢复所有寄存器 */
	ret                         /* 返回到新线程 */
