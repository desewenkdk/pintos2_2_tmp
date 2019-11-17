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



