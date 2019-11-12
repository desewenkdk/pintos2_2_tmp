#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "process.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "devices/input.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);

/*func in referrence*/
static int
get_user(const uint8_t *uaddr)
{
	int result;
	asm("movl $1f, %0; movzbl %1, %0; 1:"
		: "=&a" (result) : "m" (*uaddr));
	return result;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
    //printf ("system call handler-----------------------------------!\n");
	uint32_t choose = 0;
	choose = *(uint32_t *)(f->esp);

	/*
	check bad address at exception.c 

	bool user = 1;
	user = (f->error_code & 0x4) != 0;
	if (user == 0 || is_kernel_vaddr(f->esp))
	  exit(-1);*/ 


	if (get_user((uint8_t *)(f->esp)) == -1) {
        
		//printf("\nSeg-Fault! - access to invalid area\n");
		exit(-1);
	}

	//printf("\nSystem call_NUM:%d\n",choose);
	//hex_dump((f->esp), (f->esp), 100, 1);

	/*is any bad pointer passes here??*/
	if ((!is_user_vaddr(f->esp)) || (pagedir_get_page(thread_current()->pagedir, (f->esp)) == NULL)) {
	
		//printf("\n\n\n*************is any bad pointer passes here??***************\n\n\n");
		exit(thread_current()->exit_status);
	}
	//for test case bad-ptr address in argument
	/*else if (!is_user_vaddr(*(uint32_t*)(f->esp + 4)) || (pagedir_get_page(thread_current()->pagedir, (*(uint32_t*)(f->esp + 4))) == NULL)) {
		printf("\nInvalid address for argument\n");
		exit(thread_current()->exit_status);
	}*/


	//check esp pointer which points kernel or user : userprog/exception.c!!

	if (choose == SYS_HALT) {
		//printf("system handler-SYS_HALT");
		halt();
	}
	else if (choose == SYS_EXIT) {
		//printf("system handler-SYS_EXIT");
		if (!is_user_vaddr(f->esp + 4) || (pagedir_get_page(thread_current()->pagedir, (f->esp + 4)) == NULL ))
			exit(-1);
		else
			exit(*(uint32_t*)(f->esp + 4));
	}
	else if (choose == SYS_EXEC) {
		//printf("system handler-SYS_EXEC");
		//printf("\nSystem handler-SYS_EXEC---- argument:%p\n", *(uint32_t*)(f->esp + 4)); //�ּҰ��� �Ķ���ͷ� ���� �Լ��鿡 �����Ѵ�. esp�� ��ȿ���������� ����� �ּҰ��� invalid�� �� �ִ�.
		if (!is_user_vaddr(f->esp + 4) || (pagedir_get_page(thread_current()->pagedir, (f->esp + 4)) == NULL))
			exit(-1);

		//for test case exec-bad-ptr -> ���ʿ��ѵ�.
		else if (!is_user_vaddr(*(uint32_t*)(f->esp + 4)) || (pagedir_get_page(thread_current()->pagedir, (*(uint32_t*)(f->esp + 4))) == NULL)) {
			//printf("\nToo low address for argument\n");
			exit(-1);
		}
		else
		f->eax = exec((const char*)*(uint32_t*)(f->esp + 4));
	}
	else if (choose == SYS_WAIT) {
		//printf("\nsystem handler-SYS_WAIT: %d, %s\n",*(uint32_t *)(f->esp + 4), thread_current()->name);
		if (!is_user_vaddr(f->esp + 4) || (pagedir_get_page(thread_current()->pagedir, (f->esp + 4)) == NULL))
			exit(-1);
		else
			f->eax = wait((pid_t)(*(uint32_t*)(f->esp + 4)));
	}
	else if (choose == SYS_READ) {
		//printf("system handler-SYS_READ");
		if (!is_user_vaddr(f->esp + 4))
			exit(-1);
		else if (!is_user_vaddr(f->esp + 8))
			exit(-1);
		else if (!is_user_vaddr(f->esp + 12))
			exit(-1);

		int fd = (int)*(uint32_t *)(f->esp + 4);
		void* buffer = (void *)*(uint32_t *)(f->esp + 8);
		uint32_t size = (unsigned)*((uint32_t *)(f->esp + 12));
		f->eax = read(fd, buffer, size);

	}
	else if (choose == SYS_WRITE) {
		//printf("system handler-SYS_WRITE");
		if (!is_user_vaddr(f->esp + 4) || (pagedir_get_page(thread_current()->pagedir, (f->esp + 4)) == NULL))
			exit(-1);
		else if (!is_user_vaddr(f->esp + 8) || (pagedir_get_page(thread_current()->pagedir, (f->esp + 8)) == NULL) )
			exit(-1);
		else if (!is_user_vaddr(f->esp + 12) || (pagedir_get_page(thread_current()->pagedir, (f->esp + 12)) == NULL ))
			exit(-1);

		int fd = (int)*(uint32_t *)(f->esp + 4);
		void* buffer = (void *)*(uint32_t *)(f->esp + 8);
		uint32_t size = (uint32_t)*(uint32_t *)(f->esp + 12);
		f->eax = write(fd, buffer, size);
	}


	else if (choose == SYS_FIBO){
		if (!is_user_vaddr(f->esp + 4) || (pagedir_get_page(thread_current()->pagedir, (f->esp + 4)) == NULL))
			exit(-1);
		else{
			int n = (int)*(uint32_t*)(f->esp + 4);
			f->eax = fibonacci(n);
		}
	}

	else if (choose == SYS_SUM4){
		if (!is_user_vaddr(f->esp + 4) || (pagedir_get_page(thread_current()->pagedir, (f->esp + 4)) == NULL)){
			exit(-1);
		}
		if (!is_user_vaddr(f->esp + 8) || (pagedir_get_page(thread_current()->pagedir, (f->esp + 8)) == NULL)){
			exit(-1);
		}
		if (!is_user_vaddr(f->esp + 12) || (pagedir_get_page(thread_current()->pagedir, (f->esp + 12)) == NULL)){
			exit(-1);
		}
		if (!is_user_vaddr(f->esp + 16) || (pagedir_get_page(thread_current()->pagedir, (f->esp + 16)) == NULL)){
			exit(-1);
		}

		//for readability
		int a = (int)*(uint32_t*)(f->esp + 4);
		int b = (int)*(uint32_t*)(f->esp + 8);
		int c = (int)*(uint32_t*)(f->esp + 12);
		int d = (int)*(uint32_t*)(f->esp + 16);
		f->eax = sum_of_four_int(a,b,c,d);
	}
}
void halt(void)
{
	shutdown_power_off();
}

pid_t exec(const char *cmd_line)
{
	return process_execute(cmd_line);
}

int wait(pid_t pid)
{
	return process_wait(pid);
}

void exit(int status)
{
	//for debug printf("syscall.c_exit\n"); 
	thread_current()->exit_status = status;

	/*Don't Erase this print!!!! it's necessary for test*/
	printf("%s: exit(%d)\n", thread_name(), status);

	thread_exit();
}

int read(int fd, void *buffer, unsigned size) {
	unsigned int i;

	if(fd==0){ // standard input
		for(i=0; i< size; i++)
			*(uint8_t *) (buffer+i) = input_getc();
		return size;
	}
	else{
		return -1;
	}

	//abnormal situation
	return -1;
}

int write(int fd, const void *buffer, unsigned size) {
	int i;
	if (fd == 1) {
	//for debug
		//printf("in syscall-write : %d, 0X%p, %d\n", fd, buffer, size);
		//
		putbuf(buffer, size);
		
	//	for (i=0; i < (int)size; i++) {
	//      *(uint8_t*)(buffer+i) = input_putc();
	//		printf("%u ",*(uint8_t *)(buffer+i));
	//	    
	//	}
	//	printf("%u", *(uint8_t *)(buffer));
		return size;
	}
	return -1;
}

int fibonacci(int n){
    int fibo1 = 1, fibo2=1, tmp;
	int i=0;
	if (n == 1 || n==2)
		return 1;
    
	for (i;i<n-2;i++){
		tmp = fibo2;
		fibo2 = fibo1 + fibo2;
		fibo1 = tmp;
	}
	return fibo2;
}

int sum_of_four_int(int a,int b, int c, int d){
	return a+b+c+d;
}

