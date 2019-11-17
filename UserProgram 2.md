ritten with [StackEdit](https://stackedit.io/).
# Userprogram 제작 일지
> 20141586 20141506
 
## 1. Argument passing

##  User-Memory Stack Structure
this stack can be accessed by **esp**  `void pointer(void *)` of **intr_frame structure** defined at *thread.h*
```c
/* Interrupt stack frame. */
struct intr_frame
  {
    /* Pushed by intr_entry in intr-stubs.S.
       These are the interrupted task's saved registers. */
    uint32_t edi;               /* Saved EDI. */
    uint32_t esi;               /* Saved ESI. */
    uint32_t ebp;               /* Saved EBP. */
    uint32_t esp_dummy;         /* Not used. */
    uint32_t ebx;               /* Saved EBX. */
    uint32_t edx;               /* Saved EDX. */
    uint32_t ecx;               /* Saved ECX. */
    uint32_t eax;               /* Saved EAX. */
    uint16_t gs, :16;           /* Saved GS segment register. */
    uint16_t fs, :16;           /* Saved FS segment register. */
    uint16_t es, :16;           /* Saved ES segment register. */
    uint16_t ds, :16;           /* Saved DS segment register. */

    /* Pushed by intrNN_stub in intr-stubs.S. */
    uint32_t vec_no;            /* Interrupt vector number. */

    /* Sometimes pushed by the CPU,
       otherwise for consistency pushed as 0 by intrNN_stub.
       The CPU puts it just under `eip', but we move it here. */
    uint32_t error_code;        /* Error code. */

    /* Pushed by intrNN_stub in intr-stubs.S.
       This frame pointer eases interpretation of backtraces. */
    void *frame_pointer;        /* Saved EBP (frame pointer). */

    /* Pushed by the CPU.
       These are the interrupted task's saved registers. */
    void (*eip) (void);         /* Next instruction to execute. */
    uint16_t cs, :16;           /* Code segment for eip. */
    uint32_t eflags;            /* Saved CPU flags. */
    void *esp;                  /* Saved stack pointer. */
    uint16_t ss, :16;           /* Data segment for esp. */
  };
```
At `syscall_handler (struct intr_frame *f UNUSED) `, we get interrupt frame structure by passed argument of syscall_handler function, so we can access to `user-program stack memory` by using  `f->esp` pointer.


so we must store arguments at `User-program Stack` correctly, when we create thread.  
<br>
<br>
<br>

### Parsing argument from command line input.
 we have to parse filename, arguments in `userprog/process.c/load()` function. 
   
```c 
 bool load (const char *file_name, void (**eip) (void), void **esp)
  ```
 in parameter   *file_name,*   there are not only filename to execute but also command lines we typed, like /usr/ls -a foo bar.
so we parse this string, and assign to proper variable.
variable `real_file_name` stores filename, `argv` stores last arguments of commandline, `argc` means count of arguments.
```c
char  *real_file_name;//for parsing filename from command.
char  *next_ptr;//for store next delim pointer
char  *argv[MAX_TOKEN_NUM]; // argument variable : cmd line
int argc =  0; // argument count.
argv[0] =  strtok_r(file_name, " ", &next_ptr);
//strtok_r is more thread-safe function. 
//strtok:using own static variable old, strtok_r(s,&delim,old).

while(1){
if(argv[argc] ==  NULL)break;
	argc++;
	argv[argc] =  strtok_r(NULL," ",&next_ptr);
}
real_file_name =  argv[0];
```
<br>
<br>

### Set up Stack following *80X86 Calling Convention*
After finishing parsing arguments and filename, we have to store arguments in the user stack, following *80X86 Calling Convention.*  
when, we call function f(1,2,3), The initial stack address is arbitrary:  
+----------------+  
0xbffffe7c | 3 |  
0xbffffe78 | 2 |  
0xbffffe74 | 1 |  
0xbffffe70 | return address | `esp` <---- stack pointer  
+----------------+  


it also described in ASM code, in `lib/user/syscall.c`
```c
/*lib/user/syscall.c*/
/* Invokes syscall NUMBER, passing no arguments, 
and returns the return value as an `int'. */

#define  syscall0(NUMBER) //push only SYSCALL NUMBER on stack. 
({ \
int retval; \
asm volatile \ 
	("pushl %[number]; int $0x30; addl $4, %%esp"  \
	: "=a" (retval) \
	: [number] "i" (NUMBER) \
	: "memory"); \
	retval; \
})

  
/* Invokes syscall NUMBER, passing argument ARG0, 
and returns the return value as an `int'. */
#define  syscall1(NUMBER, ARG0) 
({ \
int retval; \
asm volatile \
("pushl %[arg0]; pushl %[number]; int $0x30; addl $8, %%esp"  \
: "=a" (retval) \
: [number] "i" (NUMBER), 
: [arg0] "g" (ARG0) \
: "memory"); \
retval; \
.....

})
```

it has to be done during loads program, after `setup_stack()` so we implemented it in `userprog/process.c/load()` function. so we can set up stack like see below.
![arg1_userprogstack](/systemcall/pic/arg-pic2.JPG)

```c
if (!setup_stack (esp))
	goto done;

uint32_t startaddr = (uint32_t)(*esp);//for debugging
//do i need optimization??
uint32_t addr_of_argv[MAX_TOKEN_NUM];//


int arg_total_len =  0;
for (i=argc-1; i >=  0; i--) {
	//get length of argument to get proper space for argument.
	int len =  strlen(argv[i]) +  1;
	arg_total_len += len;
	(*esp) -= len;//move esp amount of arguement's size
	memcpy(*esp,argv[i],len);//
	addr_of_argv[i] = (uint32_t)(*esp);
}
int wordalign =  0;

/* 4byte단위로 메모리블럭을 관리하기 위해 4byte에서 남는 영역은 0으로 채워둔다. */
//setup word align value
wordalign =  4  - (arg_total_len %  4);
uint8_t wa =  0;
for (i =  0; i < wordalign; i++) {
	(*esp) -=  1;
	**(uint8_t**)(esp) = wa;
	//printf("%u\n", (uint32_t)(*esp));
}

/*setup centinel
argument들의 주소값과 실제 argument값들을 저장한 영역을 구분해주는 값. 
4byte integer를 사용함.*/
(*esp) -=  4;
**(uint32_t**)(esp) =  0;

//setup offset of argvs
for (i = argc -  1; i >=  0; i--) {
	(*esp) -=  4;
	**(uint32_t**)(esp) =  addr_of_argv[i];
}

//argv start addr in stack
(*esp) -=  4;
**(uint32_t**)(esp) = (*esp) +  4

//argc in stack
(*esp) -=  4;

**(uint32_t**)(esp) = argc;
  
//return address
(*esp) -=  4;
**(uint32_t**)(esp) =  0;

/*printf("\ndump stack_------------------------------------------\n");
uint32_t ofs = (uintptr_t)*esp;
uint32_t bytesize = startaddr - ofs;
hex_dump(ofs,*esp,bytesize, 1); 
we can check stack inside by this....*/
```
result
![dump_stack](/systemcall/pic/echo.PNG)
<br>
<br>

## 2. User Memory Access
we simply checked three properties. 
#### 1.  esp스택 포인터가 kernel 영역을 참조하진 않는가?
#### 2.  virtual address와 실제 메모리 공간이 mapping되어있는가?
위 두가지를 확인하기 위해 `threads/vaddr.h`의 `is_user_vaddr()`function과 `userprog/pagedir.c` 의 `pagedir_get_page()`function을 사용함. 
```c
/* Returns true if VADDR is a user virtual address. */
static  inline  bool
is_user_vaddr (const  void  *vaddr){
	return vaddr < PHYS_BASE;}

/* Looks up the physical address that corresponds to user virtual address 
UADDR in PD. 
Returns the kernel virtual address corresponding to 
that physical address,
or a null pointer if UADDR is unmapped. */
void  *pagedir_get_page (uint32_t  *pd, const  void  *uaddr){
	uint32_t  *pte;
	ASSERT (is_user_vaddr (uaddr));
	pte =  lookup_page (pd, uaddr, false);
	if (pte !=  NULL  && (*pte & PTE_P) !=  0)
		return  pte_get_page (*pte) +  pg_ofs (uaddr);
	else
		return  NULL;
}
```
위의 함수들을 `userprog/syscall.c`의 `syscall_handler()`가 system call들을 호출할 때 마다 호출하여 esp가 올바른 주소를 가리키고 있는 지를 확인한다. 

#### 3. user program이 kernel memory에 접근을 하려고 하지는 않는가? 
흔히 악성프로그램이라고 불리우는 프로그램들은 종종 user program인 주제에 시스템 메모리 영역인 kernel memory space에 접근하려고 든다. 이런 괘씸한 행동을 하려는 프로그램을 막기 위해 pintos에선 `userprog/exception.c`에서 `page_fault(struct intr_frame *f)`을 통해 page fault가 일어났음을 알리고 죽여버린다. 여기서 우리는 비정상종료임을 exit() system call을 호출시킴으로서 알리는 방식으로 구현하였다. 
```c
static  void page_fault (struct intr_frame *f){
	bool not_present; /* True: not-present page, false: writing r/o page. */
	bool write; /* True: access was write, false: access was read. */
	bool user; /* True: access by user, false: access by kernel. */
	void  *fault_addr; /* Fault address. */
	...
	asm ("movl %%cr2, %0" : "=r" (fault_addr));

	/* Turn interrupts back on (they were only off 
	so that we could be assured of reading CR2
	before it  changed). */
	intr_enable ();
	/* Count page faults. */
	page_fault_cnt++;
	
	/* Determine cause. */
	not_present = (f->error_code  & PF_P) ==  0;
	write = (f->error_code  & PF_W) !=  0;
	user = (f->error_code  & PF_U) !=  0;
	
	//exit if memory is pointing kernel space
	if (user ==  0  ||  is_kernel_vaddr(fault_addr))
		exit(-1);
		
	.....
	
	kill (f);
}

```
<br>
<br>

## 3. System Call Handler
we have a System Call Handler which calls appropriate System Calls. We get `System Call Number` from `f->esp`, so we can map System Call Functions with given System Call Number.  System Call Numbers are defined at `lib/syscall_nr.h` by Enum type values.
```c
#ifndef  __LIB_SYSCALL_NR_H
#define  __LIB_SYSCALL_NR_H
/* System call numbers. */
enum{
/* Projects 2 and later. */
	SYS_HALT, /* Halt the operating system. */
	SYS_EXIT, /* Terminate this process. */
	SYS_EXEC, /* Start another process. */
	SYS_WAIT, /* Wait for a child process to die. */
	....
	//additional system call number
	SYS_FIBO,
	SYS_SUM4
}
```
we check this at `userprog/syscall.c syscall_handler()` The number is in the user-program memory, pointed by `f->esp`. So, we must do validation check of address =  `f->esp`  before mapping the numbers and System Call Functions.
```c
/*사실 이걸 구현하진 않았다.*/
static bool vaddr_valid_checker(void *p){
	return (!is_user_vaddr(p)  || \
	(pagedir_get_page(thread_current()->pagedir, (p))  ==  NULL)
}

```
```c
static  void
syscall_handler (struct intr_frame *f UNUSED){
	uint32_t choose = *(uint32_t  *)(f->esp);
	...

	/*1. validation check for f->esp*/
	if (vaddr_valid_checker(f->esp)) {
	//printf("is any bad pointer passes here??\n");
			exit(thread_current()->exit_status);
	}
	
	/*2. mapping System Call Function 
	with given System Call Number*/
	if (choose == SYS_HALT) {
		halt();
	}

	else  if (choose == SYS_EXIT) {
		if (vaddr_valid_checker(f->esp + 4))
			exit(-1);
		else
			exit(*(uint32_t*)(f->esp  +  4));
		}
	...
	}
}
```
각 System Call마다 파라미터로 받아오는 인자의 갯수와 타입이 제각각인데, 이에 따라서 참조해야 하는 스택포인터의 주소들이 달라지므로 주소에 주의하면서 접근해야 원하는 argument값들을 User Program Stack에서부터 가져올 수 있다. 또한 값을 가져오고 싶을 때에는 `void *` 형식인 f->esp를 `(uint32_t*)` 타입으로 형변환시킨 후에 다시 dereferrencing을 통해 값에 접근해야 올바른 값을 가져옴을 확인하였다. 

<br>
<br>

## 4. System Call Implementation
이번 프로젝트에서 구현한 6가지의 System Call들에 대해 자세히 알아보도록 하자.

**void halt(void)**
* System Call Number = 0, SYS_HALT
* Terminates Pintos by calling `shutdown_power_off()` (declared in `devices/shutdown.h`). This should be seldom used, because you lose some information about possible deadlock situations, etc.
```c
void  halt(void){
	shutdown_power_off();
}
```
<br>

**void exit(int status)**
* System Call Number = 1, SYS_EXIT
* Terminates the current user program, returning status to the kernel. If the process's parent waits for it (see below), this is the status that will be returned. Conventionally, a status of 0 indicates success and nonzero values indicate errors.
*  calls `process_exit()`
```c
/*in Syscall_handler()*/
else  if (choose == SYS_EXIT) {
	if (vaddr_valid_checker(f->esp + 4))
		exit(-1);
	else
	//get exit_status value from User Program stack
		exit(*(uint32_t*)(f->esp  +  4));
	}
}
...
/*exit function*/
void  exit(int  status){
	thread_current()->exit_status  = status;//update thread's exit_status
	
	/*Don't Erase this print!!!!*/
	printf("%s: exit(%d)\n", thread_name(), status);
	//calls thread_exit() function in threads/thread.c
	thread_exit();
}
...
/*thread_exit(void)*/
* Deschedules the current thread and destroys it. Never
returns to the caller. */
void thread_exit (void){
	ASSERT (!intr_context ());
	#ifdef  USERPROG
		process_exit (); //calls process_exit() in userprog/process.c
	#endif
	
	/* Remove thread from all threads list, set our status to dying, 
	and schedule another process. 
	That process will destroy us when it calls thread_schedule_tail(). */
	intr_disable ();
	list_remove (&thread_current()->allelem);
	thread_current ()->status  = THREAD_DYING;
	schedule ();
	NOT_REACHED ();
}
...
```
in `process_exit()`, we should not delete own thread before parent thread has been noticed. So, before child thread - *current thread in process_exit()* - dead, we must notice to parent thread that child thread will be dead. we implemented this by using *semaphore structure*. We will explain later about two semaphore variable : *sema_lock, sema_mem* to implement synchronization of parent and child thread. 
```c
void process_exit (void){
	struct thread *cur =  thread_current ();
	uint32_t  *pd;
	//printf("process_exit() start------------------------\n");

	/* Destroy the current process's page directory and switch back to the kernel-only page directory. */
	pd =  cur->pagedir;
	
	if (pd !=  NULL){

	/* Correct ordering here is crucial. We must set
	cur->pagedir to NULL before switching page directories,
	 so that a timer interrupt can't switch back to the
	process page directory.
	 We must activate the base page directory before destroying the process's page directory, 
	or our active page directory will be one that's been freed (and cleared). */
	cur->pagedir  =  NULL;
	pagedir_activate (NULL);
	pagedir_destroy (pd);

}

sema_up(&(cur->parent->sema_lock));/*wake up parent's semaphore.
-> child thread가 exit하는 때가 왔을 때, parent의 공유자원이 있다고 표시해줌으로서 parent thread가 다시 깨어날 수 있게 함. */
/*modify to remove in process_wait()*/
//list_remove(&(cur->ls_child_elem));
sema_down(&(cur->sema_mem));//이 때, child의 메모리 자원은 child가 exit되면서 스스로 지우지 못하도록 잠궈놓는다. 이로 인해 child 스레드를 잠시 재우는 역할을 한다.
```
<br>

#### pid_t  exec(const  char  *cmd_line)
* System Call Number = 2, SYS_EXEC
* get cmd line input from f->esp + 4, and it calls `process_execute(char *)`  in `userprog/process.c`
* `process_execute(char *)` will make thread name with filename which name is tokenized from cmd_line.
```c

/* in userprog/syscall.c system_handler() */
else  if (choose == SYS_EXEC) {
	if (!is_user_vaddr(f->esp  +  4) || \
	(pagedir_get_page(thread_current()->pagedir, (f->esp  +  4)) ==  NULL))	
		exit(-1);
	//for test case exec-bad-ptr -> esp가 가리키는 곳에 주소값이 저장되어 있고 그 주소가 유효하지 않은 경우일 때
	else  if (!is_user_vaddr(*(uint32_t*)(f->esp  +  4)) || \
	(pagedir_get_page(thread_current()->pagedir,(*(uint32_t*)(f->esp  +  4))) ==  NULL)) {
	//printf("\nToo low address for argument\n");	
		exit(-1);
	}
	else	
		f->eax  =  exec((const  char*)*(uint32_t*)(f->esp  +  4));
}

...

/* in userprog/system.c */
pid_t  exec(const  char  *cmd_line){
	return  process_execute(cmd_line);
}
```
 `process_execute(char *filename)`는 process를 생성하는 역할을 하는데,  파라미터로 받아온 filename은 파일 이름 이외에 파일 수행 옵션 및 기타 잡다한 인자들이 추가되어있는 한 줄의 command line 입력을 통째로 가져온 문자열이므로 여기서 알맞는 filename을 real_filename에 파싱해주어야 한다. 그 후 thread_create()함수에 real_filename을 파라미터로 실어서 스레드를 생성하도록 한다. <br>
 thread_create()에서 받아온 filename을 통해, init_thread()에서 스레드를 초기화해주는데, 여기서 우리는 이후에 있을 **wait system call**에서 수행될 동기화 작업을 위해 thread 구조체 내부에 멤버 변수로 semaphore구조체 두 개와 child thread를 탐색하기 위한 child list를 선언한다. 
 * semaphore구조체 중 하나는 자식 스레드가 exit system call을 통해 제거되기 전 부모에게 자식 스레드가 exit함을 알려주는 용도로 사용될 것이다. 
 	-  이를 위해 부모의 공유 자원을 막아두어 부모 스레드를 잠근다는 의미로 sema_lock이라는 이름을 사용.   
* 또 하나는 exit system call이 수행될 때, child thread가 스스로 자기 자신을 죽이면 안되고 - bad pointer등이 들어왔을 경우에 대비 - parent thread가 child를 찾아서 지우도록 해야한다. 이를 위해 exit가 수행완료되기 직전에 child thread의 공유메모리 자원을 (semaphore값을 통해) 잠궈버림으로서 메모리를 보존시키는 용도로 사용되는 sema_mem이라는 세마포어이다. 
```c
/*threads/thread.c, thread_create()*/
tid_t thread_create (const  char  *name, int  priority, thread_func *function, void  *aux){

	struct thread *t;
	...
	/* Initialize thread. */
	init_thread (t, name, priority);//initialize thread with given filename
	tid =  t->tid  =  allocate_tid ();
	...
	thread_unblock (t);
	return tid;
}

/*threads/thread.c, init_thread()*/
/* Does basic initialization of T as a blocked thread named NAME. */
static  void init_thread (struct thread *t, const  char  *name, 
int  priority){
	ASSERT (t !=  NULL);
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT (name !=  NULL);
	memset (t, 0, sizeof  *t);
	t->status  = THREAD_BLOCKED;
	strlcpy (t->name, name, sizeof  t->name);
	t->stack  = (uint8_t  *) t + PGSIZE;
	t->priority  = priority;
	t->magic  = THREAD_MAGIC;
	list_push_back (&all_list, &t->allelem);
	t->parent  =  running_thread();
	
	#ifdef  USERPROG /*initialize semaphores, and child list member variables
to implement synchronization.*/
	sema_init(&(t->sema_lock), 0); 
	sema_init(&(t->sema_mem), 0);
	//ls_child
	list_init(&(t->ls_child));
	list_push_back(&(running_thread()->ls_child),&(t->ls_child_elem));
	#endif
}
```
<br>

#### int  wait(pid_t  pid)
* System Call Number = 3, SYS_WAIT
* it returns Child thread's exit status.
 1) What wait() system call should do is wait child process until it finishes its work.
2) Check child thread ID is valid
3) Get the exit status from child thread when the child thread is dead
4) Prevent termination of process before return from wait()
	* we prevent this to implement synchronization by using two semaphores, *sema_lock*, and *sema_mem*.
```c
/*in userprog/syscall.c*/
static void syscall_handler(struct intr_frame *f UNUSED){
...
	else  if (choose == SYS_WAIT) {
		if (!is_user_vaddr(f->esp  +  4) || \
		(pagedir_get_page(thread_current()->pagedir,\
		 (f->esp  +  4)) ==  NULL))
			exit(-1);
		else
			f->eax  =  wait((pid_t)(*(uint32_t*)(f->esp  +  4)));
		}
	}
...
}
int  wait(pid_t  pid){
	return  process_wait(pid);
}
```
<br>

in `process_wait(tid_t child_tid UNUSED)` , first, we find child thread we have to wait which is finished.  We use Linked list struct - *already initialized at init_thread()* -to search child thread that has given child_tid value. 
If given child_tid value is invalid tid so thread will not exist with given child_tid,  we return -1 for child thread's exit_status
When we find Child thread correctly,  we wait until child thread finishes it's task, using `sema_down(&(tmp_childt->parent->sema_lock))` . After Child thread exits, we get exit status from child thread, and remove child list, and release child thread's memory by using `sema_up(&(tmp_childt->sema_mem))`
```c
/* process_wait() */
int process_wait (tid_t child_tid UNUSED){
	struct thread *parent_process =  thread_current();
	struct list list_child = (parent_process->ls_child);
	struct list_elem *child_list_elem;
	struct thread *tmp_childt =  NULL;
	uint32_t exit_status;
	
	//find child thread 
	for (child_list_elem =  list_begin(&list_child);\
		child_list_elem !=  list_end(&list_child);\
		child_list_elem =  list_next(child_list_elem)){
		tmp_childt =  list_entry(child_list_elem, struct thread, ls_child_elem);

		tmp_childt->name,(int)(tmp_childt->tid));

		/*현재 thread : parent의 child중 파라미터로 받아온tid값과 
		같은 값을 갖는 child thread찾기
*/
		/*if, child's tid is invalid 
		-- if child is not exist or corrupted*/
		if(tmp_childt->tid  <  0){
			return  -1;
		}

		if (tmp_childt->tid  == child_tid) {
			(int)(tmp_childt->tid), 
			(tmp_childt->name),
			(parent_process->name));
			
			sema_down(&(tmp_childt->parent->sema_lock));//parent의 sema값을 down시켜서 child가 exit될 때 까지 wait.
			//sema_down(&(tmp_childt->sema_lock));//parent가 아니라 child의 sema값을 down시켜서 child가 exit될 때 까지 wait.

			/*modify to remove child list in process_wait()*/
			exit_status = tmp_childt->exit_status;
			list_remove(&(tmp_childt->ls_child_elem));
			sema_up(&(tmp_childt->sema_mem));

			//free(tmp_childt); -> don't do that.
			return exit_status;
		}

	}
	return  -1;
}
```

<br>

#### int  write(int  fd, const  void  *buffer, unsigned  size)
* fd = 1인 경우 - standard output을 사용하는 경우- 에 대해서만 구현하였다. 
* 필요한 argument가 int, const void*, unsigned형태의 3개이므로 총 12byte의 메모리를 esp로 부터 차례대로 참조했다. 
* Standard output을 사용하는 경우, 한 번에 버퍼 크기(100byte이하)만큼 출력해주는 함수인 putbuf함수를 사용할 수 있다기에 사용하였다. 
	* Fd 1 writes to the console. Your code to write to the console should write all of buffer in one call to putbuf(), at least as long as size is not bigger than a few hundred bytes.  - pintos manual.pdf, p37	
```c
/*in userprog/syscall.c*/
static void syscall_handler(struct intr_frame *f UNUSED){
...
	else  if (choose == SYS_WRITE) {
		if (vaddr_valid_checker(f->esp + 4))
			exit(-1);
		else  if (vaddr_valid_checker(f->esp + 8))
			exit(-1);
		else  if (vaddr_valid_checker(f->esp + 12))
			exit(-1);

		int fd = (int)*(uint32_t  *)(f->esp  +  4);
		void* buffer = (void  *)*(uint32_t  *)(f->esp  +  8);
		uint32_t size = (uint32_t)*(uint32_t  *)(f->esp  +  12);
		f->eax  =  write(fd, buffer, size);
	}
.....
}
int  write(int  fd, const  void  *buffer, unsigned  size) {
	int i;
	if (fd ==  1) {
		putbuf(buffer, size);
		return size;
	}
	return  -1;
}
	
```
<br>

#### int  read(int  fd, const  void  *buffer, unsigned  size)
* write와 비슷하게, fd =0인 경우에 standard input임을 명시해여 구현하였다. 
```c
/*in userprog/syscall.c*/
static void syscall_handler(struct intr_frame *f UNUSED){
...
105     else if (choose == SYS_READ) {
106         //printf("system handler-SYS_READ");
107         if (!is_user_vaddr(f->esp + 4))
108             exit(-1);
109         else if (!is_user_vaddr(f->esp + 8))
110             exit(-1);
111         else if (!is_user_vaddr(f->esp + 12))
112             exit(-1);
113
114         int fd = (int)*(uint32_t *)(f->esp + 4);
115         void* buffer = (void *)*(uint32_t *)(f->esp + 8);
116         uint32_t size = (unsigned)*((uint32_t *)(f->esp + 12));
117         f->eax = read(fd, buffer, size);
118
119     }
}
....
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
```

<br>
<br>

## 5. Additional System Call
#### prequests
1. lib/user/syscall.h
• Write prototype of 2 new system call APIs
```c
/* lib/user/syscall.h */
...
#ifndef  __LIB_USER_SYSCALL_H
#define  __LIB_USER_SYSCALL_H
...
//user defined system call functions;
int  fibonacci(int  n);
int  sum_of_four_int(int  a, int  b, int  c, int  d);
...
#endif /* lib/user/syscall.h */
```

2. lib/syscall-nr.h
• Add system call numbers for 2 new system calls : SYS_FIBO, SYS_SUM4
```c
1 #ifndef __LIB_SYSCALL_NR_H
2 #define __LIB_SYSCALL_NR_H
3
4 /* System call numbers. */
5 enum
6 {
7     /* Projects 2 and later. */
8     SYS_HALT,                   /* Halt the operating system. */
9     SYS_EXIT,                   /* Terminate this process. */

...
32     //additional system call number
33     SYS_FIBO,
34     SYS_SUM4
35};
38 #endif /* lib/syscall-nr.h */

```

3. lib/user/syscall.c
• Define new syscall4() function for sum_of_four_int() system call API
• Define fibonacci() and sum_of_four_int() system calls APIs
```c
 64 /* Invokes syscall NUMBER, passing arguments ARG0, ARG1,ARG2, and
 65    ARG3, and returns the return value as an `int'. */
 66 #define syscall4(NUMBER, ARG0, ARG1, ARG2, ARG3)                      \
 67         ({                                                      \
 68           int retval;                                           \
 69           asm volatile                                          \
 70             ("pushl %[arg3]; pushl %[arg2]; pushl %[arg1]; pushl %[arg0]; "    \
 71              "pushl %[number]; int $0x30; addl $20, %%esp"      \
 72                : "=a" (retval)                                  \
 73                : [number] "i" (NUMBER),                         \
 74                  [arg0] "g" (ARG0),                             \
 75                  [arg1] "g" (ARG1),                             \
 76                  [arg2] "g" (ARG2),                             \
 77                  [arg3] "g" (ARG3)                              \
 78                : "memory");                                     \
 79           retval;                                               \
 80         })
....
204 int
205 fibonacci (int n){
206    return syscall1(SYS_FIBO, n);
207 }
208
209 int
210 sum_of_four_int(int a,int b,int c,int d){
211    return syscall4(SYS_SUM4, a,b,c,d);}

```

#### implement function header and body
```c
/* userprog/syscall.h */
  1 #ifndef USERPROG_SYSCALL_H
  2 #define USERPROG_SYSCALL_H
  3
  4 typedef int pid_t;
  5
  6 void syscall_init(void);
  7
  8 /*prototypes of syscall funcions*/
  9 void halt(void);
 10 void exit(int status);
 11 pid_t exec(const char *cmd_line);
 12 int wait(pid_t pid);
 13 int read(int fd, void *buffer, unsigned size);
 14 int write(int fd, const void *buffer, unsigned size);
 15
 16 //user defined system call functions;
 17 int fibonacci(int n);
 18 int sum_of_four_int(int a, int b, int c, int d);
 19
 20
 21 #endif /* userprog/syscall.h */
 ....

/* userprog/syscall.c */
 33 static void
 34 syscall_handler (struct intr_frame *f UNUSED)
 35 {
	...
136     else if (choose == SYS_FIBO){
137         if (!is_user_vaddr(f->esp + 4) || \
(pagedir_get_page(thread_current()->pagedir, (f->esp + 4)) == NULL))
138             exit(-1);
139         else{
140             int n = (int)*(uint32_t*)(f->esp + 4);
141             f->eax = fibonacci(n);
142         }
143     }
144
145     else if (choose == SYS_SUM4){
146         if (!is_user_vaddr(f->esp + 4) || \
(pagedir_get_page(thread_current()->pagedir, (f->esp + 4)) == NULL)){
147             exit(-1);
148         }
149         if (!is_user_vaddr(f->esp + 8) || \
(pagedir_get_page(thread_current()->pagedir, (f->esp + 8)) == NULL)){
150             exit(-1);
151         }
152         if (!is_user_vaddr(f->esp + 12) || \
(pagedir_get_page(thread_current()->pagedir, (f->esp + 12)) == NULL)){
153             exit(-1);
154         }
155         if (!is_user_vaddr(f->esp + 16) || \
(pagedir_get_page(thread_current()->pagedir, (f->esp + 16)) == NULL)){
156             exit(-1);
157         }
158
159         //for readability, we used variable a,b,c,d
160         int a = (int)*(uint32_t*)(f->esp + 4);
161         int b = (int)*(uint32_t*)(f->esp + 8);
162         int c = (int)*(uint32_t*)(f->esp + 12);
163         int d = (int)*(uint32_t*)(f->esp + 16);
164         f->eax = sum_of_four_int(a,b,c,d);
165     }
166 }
167 .....
228 int fibonacci(int n){
229     int fibo1 = 1, fibo2=1, tmp;
230     int i=0;
231     if (n == 1 || n==2)
232         return 1;
233
234     for (i;i<n-2;i++){
235         tmp = fibo2;
236         fibo2 = fibo1 + fibo2;
237         fibo1 = tmp;
238     }
239     return fibo2;
240 }
241
242 int sum_of_four_int(int a,int b, int c, int d){
243     return a+b+c+d;
244 }
245 ...
```

#### sum result
![sumresult](/systemcall/pic/sum.PNG)
<br>
<br>


## pass 21 test
![passresult](/systemcall/pic/pass11.PNG)
![passresult2](/systemcall/pic/pass111.PNG)




# UserProgram 2_2 implementation

## File-system related system calls.
아래와 같은 system call들을 추가로 구현하였다. 
#### System call 목록.
```c
create
remove
open
close
filesize
read
write
seek
tell
```
<br>

#### create
기본 제공하는 filesys와 file함수들을 호출함.
```c
67 static void syscall_handler (struct intr_frame *f UNUSED)
68 {
...
209     else if(choose == SYS_CREATE){
210         //if true, then invalid esp.
211         if(vaddr_valid_checker(f->esp)){
212             exit(-1);
213         }
214         if(vaddr_valid_checker(f->esp+4)){
215             exit(-1);
216         }
217         if(vaddr_valid_checker(f->esp+8)){
218             exit(-1);
219         }
220
221         f->eax = create((const char*)*(uint32_t*)(f->esp+4), (unsigned)*(uint32_t*)(f->esp+8));
222     }
...
428 bool create(const char *file, unsigned initial_size){
429     if (file == NULL)
430         exit(-1);
431     return filesys_create(file, initial_size);
432 }
....
```
<br>

#### remove
기본 제공하는 filesys와 file함수들을 호출함.
```c
67 static void syscall_handler (struct intr_frame *f UNUSED)
68 {
...
224     else if(choose == SYS_REMOVE){
225         if(vaddr_valid_checker(f->esp)){
226             exit(-1);
227         }
228         f->eax = remove((const char*)*(uint32_t*)(f->esp + 4));
229     }
...
434 bool remove(const char *file){
435     if(file == NULL)
436         exit(-1);
437     return filesys_remove(file);
438 }
...
```
<br>

#### open
기본 제공하는 filesys_open()을 통해 파일을 열었다.  
1. 파일이름이 NULL인가 에 대한 검사를 수행한다.  
2. 아니라면, lock을 통해 critical section을 보호한다.(실행못하게)
3. 연 파일이 NULL인가에 대해 검사한다. 아니라면, file descriptor값을 부여하고 thread에 종속시킨다.  
	* 이 때, 열려있는 파일에 대해 writable한지 검사한다.
4. lock을 해제한다.
5. 알맞은 file descriptor값을 리턴한다.
```c
67 static void syscall_handler (struct intr_frame *f UNUSED)
68 {
...
231     else if(choose == SYS_OPEN){
232         if(vaddr_valid_checker(f->esp))
233             exit(-1);
234         if(vaddr_valid_checker(f->esp + 4))
235             exit(-1);
236         f->eax = open((const char*)*(uint32_t*)(f->esp+4));
237     }
...
441 int open(const char *file){
442     int i,return_fd;
445
446 /*
...
455 */
456     if(file == NULL){
457    ...
459         return_fd = -1;
460     }
461
462
463     else{
464         vaddr_valid_checker(file);
465
466         //acquire before fileopen.
467         lock_acquire(&file_lock);
468         struct file *fileopen = filesys_open(file);
469         struct thread *cur_t = thread_current();
470
471 /*
472     	sema_down(&mutex);
473     	readcount++;
474     	if (readcount == 1){
475     	    sema_down(&wrt);
476    	 	}
477    	 	sema_up(&mutex);
478 */
479
...
482
483
484         if (fileopen == NULL){
...
487             return_fd = -1;
489         }
490     //iteration start with fd = 3; fd 0,1,2 is already defined for (STDIN_FILENO) , (STDOUT_FILE    NO), STDERR.
491
492 	/*
493     	sema_down(&mutex);
494     	readcount++;
495     	if (readcount == 1){
496         	sema_down(&wrt);
497     	}
498     	sema_up(&mutex);
499 	*/
500         else{
501             for(i=3; i<131; i++){
502                 if(cur_t->files[i] == NULL){
505                     if (strcmp(thread_name(), file) == 0){

506 //executable한 파일은 write못 하도록 처리.
508                        file_deny_write(fileopen);
509 //checking rox-child, is file in current thread is closed? 만약 그렇다면 NULL체크도 했어야 했다.
510                 //printf("is denyed?? %s %s\n", cur_t->name, file);
511 //              printf("%s fd is %d\n", file, i);
512                     }
513             //update current file, here. not upper
514                     cur_t->files[i] = fileopen;
515                     return_fd = i;
516                     break;
517                 }
518             }
519         }
520         lock_release(&file_lock);
...
530     }
531     return return_fd;
532 }

```
<br>

#### close
기본 제공하는 file_close()을 통해 파일을 열었다.  
1. 파일이 이미 thread에서 닫혀있다 처리되어 있는지 검사한다. 
2. 아니라면, 파일을 닫고 thread에서도 닫혀있다 알 수 있도록 NULL처리 해준다.
```c
67 static void syscall_handler (struct intr_frame *f UNUSED)
68 {
...
259     else if(choose == SYS_CLOSE){
260         if(vaddr_valid_checker(f->esp))exit(-1);
261         if(vaddr_valid_checker(f->esp+4))exit(-1);
262         close((int)*(uint32_t*)(f->esp + 4));
263     }
...
559 void close(int fd){
560     struct thread *cur = thread_current();
561     struct file *f_to_close = cur->files[fd];
562
563
564     if (f_to_close == NULL)
565         exit(-1);
566
567     //before close your file, make sure to thread->files[fd] make NULL!! file_close can't do tha    t.
568     else{
569         //these tasks are executing in file_close function.
570         file_allow_write(cur->files[fd]);
571         //cur->files[fd] = NULL;
572         //닫기 전에 file deny처리 확인
573 //      printf("syscall-close t:%s can write? %s\n" ,cur->name, cur->files[fd]->deny_write ? "fa    lse" : "true");
574
575
576 //      printf("fd : %d", fd);
577 //      printf("aaa %s %s\n",cur->name, cur->parent->name);
578         file_close(f_to_close);
579         cur->files[fd] = NULL;//항상 닫았으면, thread가 갖고 있는 파일 (정보)도 NULL로 만들어서     닫혔다고 인식시키자.
580 //      printf("%d fd is closed\n",fd);
581     }
582 }


```
<br>

#### filesize 
해당 파일이 이미 닫혀있는지 확인한 뒤 기본 제공되는 file_length()함수를 사용하였다. 
```c
67 static void syscall_handler (struct intr_frame *f UNUSED)
68 {
...
239     else if(choose == SYS_FILESIZE){
240         if(vaddr_valid_checker(f->esp) || \
			vaddr_valid_checker(f->esp + 4)){
241             exit(-1);
242         }
243         f->eax = filesize((int)*(uint32_t*)(f->esp + 4));
244     }
...
534 int filesize(int fd){
535     struct thread *cur = thread_current();
536     if(cur->files[fd] == NULL)
537         exit(-1);
538     return file_length(cur->files[fd]);
539 }
...
```
<br>

#### read : fd > 2인 경우
file descriptor값이 0,1,2보다 큰 경우에 대한 처리를 추가하였다.  
read에서는 lock을 통해 critical section접근을 관리하지 않아도 괜찮다. 파일을 읽는 건 다수의 스레드가 접근해도 상관이 없다.
```c
308 int read(int fd, void *buffer, unsigned size) {
309     unsigned int i;
...
  329     if(fd==0){ // standard input
  330         for(i=0; i< size; i++)
  331             *(uint8_t *) (buffer+i) = input_getc();
  332         return size;
  333     }
  334
  335     else if (fd > 2 && fd < 131){
  336
  337
  338         //return the number of bytes actually read, file.c
  339         //debug
  340         //printf("-------------------File descripter : %d %x-------------------\n",fd,fd);
  341         struct thread *cur = thread_current();
  342         if(cur->files[fd] == NULL)
  343             exit(-1);
  344
  345
  346         num_readbyte = file_read(cur->files[fd], buffer, size);
  347
  348     }
  349
  350     else{
  351         num_readbyte =  -1;
  352     }

```
<br>

#### write : fd > 2인 경우
file descriptor값이 0,1,2보다 큰 경우에 대한 처리를 추가하였다. 
또한 이미 열려있는 / 수정중인 파일을 다른 스레드가 write하는 작업을 막기 위해 lock을 통해 관리하였다.
```c
  369 int write(int fd, const void *buffer, unsigned size) {
  370     int i,ret;
  371     //printf("1111111111111111111\n");
  372
  373     //sema_down(&wrt);
  374     lock_acquire(&file_lock);
  375     if(!is_user_vaddr(buffer)){
  376         exit(-1);
  377     }
  378     if (fd == 1) {
  379     //for debug
  380         //printf("in syscall-write : %d, 0X%p, %d\n", fd, buffer, size);
  381         //hex_dump(buffer, buffer, 50 , 1);
  382         putbuf(buffer, size);
  383
  390
  391     // check fd=1로 들어오는 syscall : 실행파일인 경우 cmd입력으로 처리되어 종종일루 온다.
  392     //      printf("syscall-write synread here? %s\n", thread_current()->name);
  393         ret = size;
  394     }
  395
  396     else if (fd > 2 && fd < 131){
  397     //check for access to NULL file pointer
  398         struct thread *cur = thread_current();
  399         if (cur->files[fd] == NULL){
  400             //printf("?????????? %s\n", thread_current()->name);
  401             //sema_up(&wrt);
  402             exit(-1);
  403         }
  404
  405         //block to write when this file is being written
  406         if(cur->files[fd]->deny_write){
  407         //  printf("syscall-write deny???synread?? %s", thread_current()->name);
  408             file_deny_write(cur->files[fd]);
  409         }
  410
  411         //int ret;
  412         /*returns the number of bytes actually written, file.c*/
  413         ret = file_write(cur->files[fd],buffer,size);
...
  418     }
  419     else{
  420         ret = -1;
  421     }
  422     lock_release(&file_lock);
  423     //  sema_up(&wrt);
  424 //  printf("%d\n",ret);
  425     return ret;
  426 }

```
<br>

#### seek
file_seek함수를 이용하여 수행하였다.
파일이 이미 닫혀있는 지에 대한 검사를 선행했다.
```c
67 static void syscall_handler (struct intr_frame *f UNUSED)
68 {
...
246     else if(choose == SYS_SEEK){
247         if(vaddr_valid_checker(f->esp))exit(-1);
248         if(vaddr_valid_checker(f->esp + 4))exit(-1);
249         if(vaddr_valid_checker(f->esp + 8))exit(-1);
250         seek((int)*(uint32_t*)(f->esp + 4), (unsigned)*(uint32_t*)(f->esp + 8));
251     }
...
541 void seek(int fd, unsigned position){
542     struct thread *cur = thread_current();
543     struct file *f_to_see = cur->files[fd];
544     if (cur -> files[fd] == NULL)
545         exit(-1);
546     //printf("rox-child\n");
547     file_seek(f_to_see,position);
548 }
```
<br>

#### tell
file_tell함수를 이용하여 수행하였다.
파일이 이미 닫혀있는 지에 대한 검사를 선행했다.
```c
67 static void syscall_handler (struct intr_frame *f UNUSED)
68 {
...
253     else if(choose == SYS_TELL){
254         if(vaddr_valid_checker(f->esp))exit(-1);
255         if(vaddr_valid_checker(f->esp+4))exit(-1);
256         f->eax = tell((int)*(uint32_t*)(f->esp + 4));
257     }
...
550 unsigned tell(int fd){
551     struct thread *cur = thread_current();
552     struct file *f_to_tell = cur->files[fd];
553     if (f_to_tell == NULL)
554         exit(-1);
555
556     return file_tell(f_to_tell);
557 }
```
<br>
<br>

## Managing writability of files
### Denying Writes to Executable files 
from pintos manual
* Add code to deny writes to files in use as executables. Many OSes do this because of the unpredictable results if a process tried to run code that was in the midst of being changed on disk. You can use file_deny_write() to prevent writes to an open file. Calling file_allow_write() on the file will re-enable them (unless the file is denied writes by another opener).

```syscall.c - open```System call에서 만약 현재 열고자 하는 파일이 실행파일인 경우 ```file_deny_write()```를 통해 파일 write를 방지한다.
```c
441 int open(const char *file){
...
500         else{
501             for(i=3; i<131; i++){
502                 if(cur_t->files[i] == NULL){
503 //check for filename and thread name for rox
504             //printf("%s %s\n", cur_t->name, file);
505                     if (strcmp(thread_name(), file) == 0){
506 //file write deny처리가 안 되어있나 확인
507 //              printf("syscall-open denied open %s\n",thread_current()->name);
508                         file_deny_write(fileopen);
512                 }
				}
			}
...
```
<br>

### Denying Writes that already being written
line 406
```c
369 int write(int fd, const void *buffer, unsigned size) {
...
396     else if (fd > 2 && fd < 131){
397     //check for access to NULL file pointer
398         struct thread *cur = thread_current();
399         if (cur->files[fd] == NULL){
400             //printf("?????????? %s\n", thread_current()->name);
401             //sema_up(&wrt);
402             exit(-1);
403         }
404
405         //block to write when this file is being written
406         if(cur->files[fd]->deny_write){
407         //  printf("syscall-write deny???synread?? %s", thread_current()->name);
408             file_deny_write(cur->files[fd]);
409         }
410
...
425     return ret;
426 }
```
<br>

### Allows write to file after file closed
```c
559 void close(int fd){
...
564     if (f_to_close == NULL)
565         exit(-1);
566
567     //before close your file, make sure to thread->files[fd] make NULL!! file_close can't do t    hat.
568     else{
569         //these tasks are executing in file_close function.
570         file_allow_write(cur->files[fd]);
...
578         file_close(f_to_close);
579         cur->files[fd] = NULL;//항상 닫았으면, thread가 갖고 있는 파일 (정보)도 NULL로 만들어>    서 닫혔다고 인식시키자.
580 //      printf("%d fd is closed\n",fd);
581     }
582 }
```

<br>

## Managing Critical Section
### 1. Using Lock to protect being written and opened files.

```synch.h```에 선언된 ```lock```구조체를 활용하여 이미 다른 스레드에 의해 열려있거나 write되고 있는 파일에의 접근을 한 스레드 씩만 가능하도록, 이미 점유중인 경우 lock을 통해 다른 스레드가 점유하지 못하도록 하였다. 

```c
369 int write(int fd, const void *buffer, unsigned size) {
370     int i,ret;
371     //printf("1111111111111111111\n");
374     lock_acquire(&file_lock);
        ...
    	/* file write process */
		... 	
422     lock_release(&file_lock);
423     //  sema_up(&wrt);
424 //  printf("%d\n",ret);
425     return ret;
426 }
```

```c
441 int open(const char *file){
442     int i,return_fd;
...
    //after check filename not null...
456     if(file == NULL){
459         return_fd = -1;
460     }
461
462
463     else{
464         vaddr_valid_checker(file);
465
466         //acquire before fileopen./
467         lock_acquire(&file_lock);
    		...
            /* file open process..*/
            ...
520         lock_release(&file_lock);
    
521 /* 사실 강의자료의 reader-writer problem을 해결하듯이 semaphore로 해결해 보려 했으나 여의치 않아 간단하게 lock으로 잠그고 푸는 방식으로 구현하였다. 
522     sema_down(&mutex);
523     readcount--;
524     if(readcount == 0){
525         printf("syscall-open readcount 0 %s\n", thread_current()->name);
526         sema_up(&wrt);
527     }
528     sema_up(&mutex);
529 */
        
530     }
531     return return_fd;
532 }
```



## Managing Synchornization

#### what does pintos test does?? and what should we do??

같은 이름의 child thread를 거듭 실행시키는데, 정상적인 과정을 따른다면, 부모 스레드의 *wait* system call 호출을 통해 child thread의 exit status를 받아와야 한다. 중간에 child thread가 생성되기도 전에 부모 스레드의 자식 스레드 생성 과정이 종료된다거나 child thread가 정상적으로 load되지 못했는데 부모 스레드가 자식 스레드의 *execution*을 예외처리 없이 종료한다거나 하는 등의 비정상적인 사이클이 생겨나지 않도록 스레드 내부 멤버로 semaphore와 flag값들을 이용하여 적절한 동기화 작업과 예외처리를 수행하여야 한다.

1. 부모 스레드의 execution holding.
   * 부모스레드가 자식스레드를 create한 뒤 자식스레드의 load가 종료될 때 까지 부모스레드의 자원을 *sema_lock*값을 down시켜놓아 잠궈둔다.
   * 이후 자식스레드가 성공적으로 load된 경우에 해당 자식 스레드의 부모스레드 내부의 *sema_lock*값을 up시켜서 부모 스레드의 child thread execution이 다시 수행될 수 있도록 한다.

2. 자식 스레드의 load 실패에 대한 예외처리.

   * 만약, 1.번에서 자식스레드가 load에 실패하는 경우, 해당 부모 스레드의 *exec_success* flag값을false로 세팅하고 exit로 자식스레드 start_process(생성)를 종료시킨다. 
   * 위에서 세팅된 *exec_success*값을  부모스레드의 자식 스레드 execution수행 마지막에 확인해야 하는데, 이는 자식이 제대로 생성되어 load되었는지를 알기 위함이다. 생성되었다고 부모에 등록된 자식 스레드들을 리스트 순회를 통해 검사하면서 만약 부모 스레드에 저장시킨 exec_success값이 false라면, child process load에 실패했다는 뜻이므로 해당 child thread의 tid값에 관한 ```exit_status```를 ```process_wait```을 통해  받아오도록 함과 동시에 해당 child thread에 대한 *exit*와 메모리 반환 또한 수행되도록 한다.

   ```c
    /*in process.c*/
    33 tid_t
    34 process_execute (const char *file_name) 
    36 {
    39   char *fn_copy;
    40   tid_t tid;
    41   char *real_filename;
    42   char *nextpointer = NULL;
    43     struct list_elem *e;
    44     struct thread *t;
   ...
    59   if (filesys_open(real_filename) == NULL){
    60 //syn-read가 일루 빠지면서 계속 안 되었었다. 아무래도 파일을 다 쓰기도 전에 계
       속 파일read에서 열려고 접근했는지 뭔지 사실 잘 모르겠다.
    61 //      printf("process execute file open returns NULL syn-read here?======= %    s %s\n%s\n", thread_current()->name, real_filename, file_name);
    62         return -1;
    63     }
    64   /* Create a new thread to execute FILE_NAME. */
    69     tid = thread_create (real_filename, PRI_DEFAULT, start_process, fn_copy);
    70 //  printf("in process_Exec, filename : %s, curr_t : %s\n", real_filename, cur    ->name);
    71
    74     /*2_2 lock parent process until child process starts!!*/
    75     sema_down(&(cur->sema_load));
   ...
    84   if (tid == TID_ERROR){
    85         palloc_free_page (fn_copy);
    86 //      printf("process-exec is error??\n");
    87     }
    88     
           /*성공적으로 load되지 못한 child thread가 있는지 검사한다. 있다면 해당 tid에 대해 process_wait을 수행한다.*/
    89       for (e = list_begin(&thread_current()->ls_child); e != list_end(&thread_    current()->ls_child); e = list_next(e)) {
    90     t = list_entry(e, struct thread, ls_child_elem);
    91       if (t->parent->exec_success == false) {/*사실 순회하는 자식 들에 대해 부모는 하나뿐인데 굳이 순회를 돌아야 할까 하는 의문이 들지만 확실하게 하도록 하자.
    			*/  //printf("process-exec tid error?? %d %s\n",tid, t->name);
    93         return process_wait(tid);
    94       }
    95   }
    96     return tid;
    97 }
   ```

   ```c
   /* in process.c */
   101 static void
   102 start_process (void *file_name_)
   103 {
   104   char *file_name = file_name_;
   105   struct intr_frame if_;
   106   bool success;
   ...
   115   success = load (file_name, &if_.eip, &if_.esp);
   116
   117 //  load가 성공적으로 되었는가?
   118 //  printf("start process,load success?? %s %s\n ", file_name, thread_current(    )->name);
   119     palloc_free_page(file_name);
   120     sema_up(&(thread_current()->parent->sema_load));
   121 //부모의sema값을 올려 준 뒤에 오는 곳. 이 sema_load는 부모가자식 실행전에 먼저 끝나는 것을 막으려고 잠궈놨던 것이다.
   122 //  printf("STARt_process after sema_load up  currthread:%s\n", thread_current    ()->name);
   123
   124     if(!success){
   125         flag_for_ct= true;
   126         thread_current()->parent->exec_success = false;
   127         exit(-1);
   128     }
   135   asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
   136   NOT_REACHED ();
   137 }
   
   ```

   

# Result

<img src="C:\Users\desewenkdk\Google 드라이브\7gkrrl\운영체제\2\pass22.PNG" alt="pass22"  />

<img src="C:\Users\desewenkdk\Google 드라이브\7gkrrl\운영체제\2\pass222.PNG" alt="pass222"  />



