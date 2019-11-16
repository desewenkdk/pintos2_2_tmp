#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include <stdbool.h>
#include "userprog/syscall.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "process.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "devices/input.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/off_t.h"
#include "filesys/inode.h"
#include "threads/synch.h"

static void syscall_handler(struct intr_frame*);
static bool vaddr_valid_checker(void *p);

//for implement reader-writer synchronization(to solve reader-writer problem)
//struct semaphore mutex, wrt;
//int readcount;
struct lock file_lock;

struct file{
	struct inode *inode;
	off_t pos;
	bool deny_write;
};

/*func for check bad stack pointer*/
static bool vaddr_valid_checker(void *p){
	
	//printf("NULL?????\n");
	return (!is_user_vaddr(p)  ||\
	 (pagedir_get_page(thread_current()->pagedir, (p))  ==  NULL));
	
}

/*func in referrence*/
static int get_user(const uint8_t *uaddr)
{
	int result;
	asm("movl $1f, %0; movzbl %1, %0; 1:"
		: "=&a" (result) : "m" (*uaddr));
	return result;
}

void syscall_init(void) 
{
	//from 2_2 initialize semaphores for read-write sync.
	/*
	sema_init(&wrt, 1);
	sema_init(&mutex, 1);
	readcount = 0;
*/
	lock_init(&file_lock);
  	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
	
}

static void syscall_handler (struct intr_frame *f UNUSED) 
{
    printf ("system call handler-----------------------------------!\n");
	uint32_t choose = 0;
	choose = *(uint32_t *)(f->esp);
	/*
	check bad address at exception.c 

	bool user = 1;
	user = (f->error_code & 0x4) != 0;
	if (user == 0 || is_kernel_vaddr(f->esp))
		exit(-1);*/ 

	/*no use??*/
	if (get_user((uint8_t *)(f->esp)) == -1) {
        
		//printf("\nSeg-Fault! - access to invalid area\n");
		exit(-1);
	}
	
	
	printf("System call_NUM:%d\n\n",choose);
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
	///printf("choose %d ------- %d\n", choose, SYS_EXEC);
	if (choose == SYS_HALT) {
		//printf("system handler-SYS_HALT");
		halt();
	}
	else if (choose == SYS_EXIT) {
	//	printf("system handler-SYS_EXIT");
		if (!is_user_vaddr(f->esp + 4) || (pagedir_get_page(thread_current()->pagedir, (f->esp + 4)) == NULL ))
			exit(-1);
		else
			exit(*(uint32_t*)(f->esp + 4));
	}
	else if (choose == SYS_EXEC) {
	//	printf("system handler-SYS_EXEC");
		//printf("\nSystem handler-SYS_EXEC---- argument:%p\n", *(uint32_t*)(f->esp + 4)); //�ּҰ��� �Ķ���ͷ� ���� �Լ��鿡 �����Ѵ�. esp�� ��ȿ���������� ����� �ּҰ��� invalid�� �� �ִ�.
		printf("syn-readdddddddd\n");
		if (!is_user_vaddr(f->esp + 4) || (pagedir_get_page(thread_current()->pagedir, (f->esp + 4)) == NULL)){
	//		printf("111111111111111111111111111111\n");
			exit(-1);
		}
		//for test case exec-bad-ptr -> ���ʿ��ѵ�.
		else if (!is_user_vaddr(*(uint32_t*)(f->esp + 4)) || (pagedir_get_page(thread_current()->pagedir, (*(uint32_t*)(f->esp + 4))) == NULL)) {
			//printf("\nToo low address for argument\n");
	//		printf("222222222222222222222222222222\n");
			exit(-1);
		}
		else{
			printf("syn-read %s\n",thread_current()->name);
	//		printf("33333333333333333333333333333333\n");
			f->eax = exec((const char*)*(uint32_t*)(f->esp + 4));
		}
	}
	else if (choose == SYS_WAIT) {
		//printf("\nsystem handler-SYS_WAIT: %d, %s\n",*(uint32_t *)(f->esp + 4), thread_current()->name);
		if (!is_user_vaddr(f->esp + 4) || (pagedir_get_page(thread_current()->pagedir, (f->esp + 4)) == NULL))
			exit(-1);
		else
			f->eax = wait((pid_t)(*(uint32_t*)(f->esp + 4)));
	}
	else if (choose == SYS_READ) {
	//	printf("system handler-SYS_READ");
		if (vaddr_valid_checker(f->esp + 4))
			exit(-1);
		else if (vaddr_valid_checker(f->esp + 8))
			exit(-1);
		else if (vaddr_valid_checker(f->esp + 12))
			exit(-1);
		
		//printf("---------왜 3??? ------ %d %x\n",*(uint32_t *)(f->esp + 4), *(uint32_t *)(f->esp+4));
		int fd = (int)*(uint32_t *)(f->esp + 4);
		void* buffer = (void *)*(uint32_t *)(f->esp + 8);
		uint32_t size = (unsigned)*((uint32_t *)(f->esp + 12));
		//printf("---------왜 3??? ------ %d %x\n",fd,fd);
		//hex_dump(f->esp, f->esp, 100, 1);
		
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
		printf("readsyn %s\n", thread_current()->name);
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

	else if(choose == SYS_CREATE){
		//if true, then invalid esp.
		if(vaddr_valid_checker(f->esp)){
			exit(-1);
		}
		if(vaddr_valid_checker(f->esp+4)){
			exit(-1);
		}
		if(vaddr_valid_checker(f->esp+8)){
			exit(-1);
		}
		
		f->eax = create((const char*)*(uint32_t*)(f->esp+4), (unsigned)*(uint32_t*)(f->esp+8));
	}

	else if(choose == SYS_REMOVE){
		if(vaddr_valid_checker(f->esp)){
			exit(-1);
		}
		f->eax = remove((const char*)*(uint32_t*)(f->esp + 4));
	}

	else if(choose == SYS_OPEN){
		if(vaddr_valid_checker(f->esp))
			exit(-1);
		if(vaddr_valid_checker(f->esp + 4))
			exit(-1);
		f->eax = open((const char*)*(uint32_t*)(f->esp+4));
	}

	else if(choose == SYS_FILESIZE){
		if(vaddr_valid_checker(f->esp) || vaddr_valid_checker(f->esp + 4)){
			exit(-1);
		}
		f->eax = filesize((int)*(uint32_t*)(f->esp + 4));
	}

	else if(choose == SYS_SEEK){
		if(vaddr_valid_checker(f->esp))exit(-1);
		if(vaddr_valid_checker(f->esp + 4))exit(-1);
		if(vaddr_valid_checker(f->esp + 8))exit(-1);
		seek((int)*(uint32_t*)(f->esp + 4), (unsigned)*(uint32_t*)(f->esp + 8));
	}
	
	else if(choose == SYS_TELL){
		if(vaddr_valid_checker(f->esp))exit(-1);
		if(vaddr_valid_checker(f->esp+4))exit(-1);
		f->eax = tell((int)*(uint32_t*)(f->esp + 4));
	}

	else if(choose == SYS_CLOSE){
		if(vaddr_valid_checker(f->esp))exit(-1);
		if(vaddr_valid_checker(f->esp+4))exit(-1);
		close((int)*(uint32_t*)(f->esp + 4));
	}


}
void halt(void)
{
	shutdown_power_off();
}

pid_t exec(const char *cmd_line)
{
	printf("synread syscall.c exec calling %s\n",cmd_line);
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
	

	//close file when thread exits
	int i;
	for(i=3; i<131; i++){
		if (thread_current()->files[i] != NULL){

//to check fd
//			printf("in exit  fd:  %d\n",i);
			close(i);
		}
	}
	thread_exit();
}

int read(int fd, void *buffer, unsigned size) {
	unsigned int i;
	int num_readbyte;
	if (!is_user_vaddr(buffer)){
		//return -1; kill process!!!
		exit(-1);
	}
/*
		sema_down(&mutex);
		readcount++;
		if (readcount == 1){
			sema_down(&wrt);
		}
		sema_up(&mutex);
*/	
	if(fd==0){ // standard input
		for(i=0; i< size; i++)
			*(uint8_t *) (buffer+i) = input_getc();
		return size;
	}
	
	else if (fd > 2 && fd < 131){
		
	
		//return the number of bytes actually read, file.c
		//debug
		//printf("-------------------File descripter : %d %x-------------------\n",fd,fd);
		struct thread *cur = thread_current();
		if(cur->files[fd] == NULL)
			exit(-1);
	

		num_readbyte = file_read(cur->files[fd], buffer, size);
		
	}	

	else{
		num_readbyte =  -1;
	}
/*
	sema_down(&mutex);
	readcount--;
	if(readcount == 0){
		sema_up(&wrt);
	}
	sema_up(&mutex);
*/

	return num_readbyte;
}

int write(int fd, const void *buffer, unsigned size) {
	int i,ret;
	//printf("1111111111111111111\n");
	
	sema_down(&wrt);
	if(!is_user_vaddr(buffer)){
		exit(-1);
	}
	if (fd == 1) {
	//for debug
		//printf("in syscall-write : %d, 0X%p, %d\n", fd, buffer, size);
		//hex_dump(buffer, buffer, 50 , 1);
		putbuf(buffer, size);
		
	//	for (i=0; i < (int)size; i++) {
	//      *(uint8_t*)(buffer+i) = input_putc();
	//		printf("%u ",*(uint8_t *)(buffer+i));
	//	    
	//	}
	//	printf("%u", *(uint8_t *)(buffer));
		printf("syscall-write synread here? %s\n", thread_current()->name);
		ret = size;
	}

	else if (fd > 2 && fd < 131){
	//check for access to NULL file pointer
		struct thread *cur = thread_current();
		if (cur->files[fd] == NULL){
			printf("?????????? %s\n", thread_current()->name);
			//sema_up(&wrt);
			exit(-1);
		}

		//block to write when this file is being written
		if(cur->files[fd]->deny_write){
			printf("syscall-write deny???synread?? %s", thread_current()->name);
			file_deny_write(cur->files[fd]);
		}		

		int ret;
		/*returns the number of bytes actually written, file.c*/
		ret = file_write(cur->files[fd],buffer,size);
		printf("syscall-write before semaup %s %d\n", cur->name, ret);
	//	sema_up(&wrt);
		printf("syscall-write synread %s success?? write : %d\n", cur->name, ret);
	//	return ret;
	}
	else{
		ret = -1;
	}
	sema_up(&wrt);	
	return ret;
}

bool create(const char *file, unsigned initial_size){
	if (file == NULL)
		exit(-1);
	return filesys_create(file, initial_size);
}

bool remove(const char *file){
	if(file == NULL)
		exit(-1);
	return filesys_remove(file);
}

/*it must return "file descripter" value from created file -> index of files array*/
int open(const char *file){
	int i,return_fd = -1;
	printf("syscall-open file : %s\n", file);
	printf("syscall-open %s\n", thread_current()->name);
	
	if(file == NULL){
		printf("in syscall-open file %s failed open, NULL t : %s", file, thread_current()->name);
		//exit(-1);
		return_fd = -1;
	}

	vaddr_valid_checker(file);	
	struct file *fileopen = filesys_open(file);
	struct thread *cur_t = thread_current();
/*
	sema_down(&mutex);
	readcount++;
	if (readcount == 1){
		sema_down(&wrt);
	}
	sema_up(&mutex);
*/	
	//file is not exist, return -1 
	//printf("file position : %d", fileopen->pos);
	if (fileopen == NULL){
		//must return open function....directly before locked.
		printf("syscall-open fileopen NULL %s\n",thread_current()->name);
		return_fd = -1;
	}
	//iteration start with fd = 3; fd 0,1,2 is already defined for (STDIN_FILENO) , (STDOUT_FILENO), STDERR.

/*
	sema_down(&mutex);
	readcount++;
	if (readcount == 1){
		sema_down(&wrt);
	}
	sema_up(&mutex);
*/	

	for(i=3; i<131; i++){
		if(cur_t->files[i] == NULL){
//check for filename and thread name for rox
			//printf("%s %s\n", cur_t->name, file);
			if (strcmp(thread_name(), file) == 0){
				printf("syscall-open denied open %s\n",thread_current()->name);
				file_deny_write(fileopen);
//checking rox-child, is file in current thread is closed? 만약 그렇다면 NULL체크도 했어야 했다.
				//printf("is denyed?? %s %s\n", cur_t->name, file);
//				printf("%s fd is %d\n", file, i);
			}
			//update current file, here. not upper	
			cur_t->files[i] = fileopen;
			return_fd = i;
			break;
		}
	}

/*	
	sema_down(&mutex);
	readcount--;
	if(readcount == 0){
		printf("syscall-open readcount 0 %s\n", thread_current()->name);
		sema_up(&wrt);
	}
	sema_up(&mutex);
*/
	return return_fd;	 
}

int filesize(int fd){
	struct thread *cur = thread_current();
	if(cur->files[fd] == NULL)
		exit(-1);
	return file_length(cur->files[fd]);
}

void seek(int fd, unsigned position){
	struct thread *cur = thread_current();
	struct file *f_to_see = cur->files[fd];
	if (cur -> files[fd] == NULL)
		exit(-1);	
	//printf("rox-child\n");
	file_seek(f_to_see,position);
}

unsigned tell(int fd){
	struct thread *cur = thread_current();
    struct file *f_to_tell = cur->files[fd];
	if (f_to_tell == NULL)
		exit(-1);

    return file_tell(f_to_tell);
}

void close(int fd){
	struct thread *cur = thread_current();
	struct file *f_to_close = cur->files[fd];

	
	if (f_to_close == NULL)
		exit(-1);
	
	//before close your file, make sure to thread->files[fd] make NULL!! file_close can't do that.
	else{
		//these tasks are executing in file_close function.
		file_allow_write(cur->files[fd]);
		//cur->files[fd] = NULL;
		printf("syscall-close t:%s can write? %s\n" ,cur->name, cur->files[fd]->deny_write ? "false" : "true");


//		printf("fd : %d", fd);
//		printf("aaa %s %s\n",cur->name, cur->parent->name);
		file_close(f_to_close);
		cur->files[fd] = NULL;//항상 닫았으면, thread가 갖고 있는 파일 (정보)도 NULL로 만들어서 닫혔다고 인식시키자.
//		printf("%d fd is closed\n",fd);
	}
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

