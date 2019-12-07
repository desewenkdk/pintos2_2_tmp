#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#ifdef USERPROG
#include "userprog/process.h"

#endif
#include "threads/float_fixedpoint.c"

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);//initializing thread
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* alarm-clock 
make sleep list which make THREAD_BLOCK state until wake time comes
*/
struct list sleep_list;
/* variable stores first-wake thread's wake time*/
int64_t first_wake_tick;

/* flag for aging */
bool thread_prior_aging;

/* load_avg value*/
int load_avg;

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

	//initialize new list : sleep list
	list_init(&sleep_list);


  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
	//set priority default

	/*set load_avg*/
	load_avg = 0;  
  init_thread (initial_thread, "main", PRI_DEFAULT);

  /*initialize thread's properties*/
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();

  initial_thread->recent_cpu = 0;
  initial_thread->nice = 0;
}

bool wakeup_first_with_priority(struct list_elem *t1, struct list_elem *t2, void *noused UNUSED){
	struct thread *comp_t1 = list_entry(t1, struct thread, elem);
	struct thread *comp_t2 = list_entry(t2, struct thread, elem);

	//compare waketime of two thread, if t1 is earlier, return true;
	if(comp_t1->waketime < comp_t2->waketime){
		return true;
	}
	/* we have to consider situation that two thread has "same wake-time!!" */
	else if(comp_t1->waketime == comp_t2->waketime){
		//then, we should compare priority of two thread
		if(comp_t1->priority > comp_t2->priority){
			return true;
		}
		else return false;
	}
	else{
		return false;
	}
}



void thread_aging(){
	/* performs priority aging technique */
	struct thread *traverse;
	struct list_elem *e;

	for(e = list_begin(&ready_list); e != list_end(&ready_list); e = list_next(e))
	{
		traverse = list_entry(e, struct thread, elem);
		if(traverse->priority == PRI_MAX)
			continue;
		else if(traverse -> priority != PRI_MAX)
			traverse -> priority += 1;
	}

	//re-schedule ready_list
	list_sort(&ready_list, thread_comp_priority, NULL);
}

/* make thread BLOCK state until wakeup tick comes*/
void thread_make_sleep(int64_t tick){
	struct thread *cur = thread_current();
	ASSERT(cur != idle_thread);
//    int64_t start = timer_ticks();
	
	//disable interrupt during putting to sleep
	enum intr_level prev_lv; // before disable interrupt, save current interrupt level
	prev_lv = intr_disable();

	/*파라미터로 받아오는 tick값에 start+tick값을 넣어서 가져오자.*/
	cur->waketime = tick;
	//insert sleep thread, list will sorted by waketime.
	list_insert_ordered(&sleep_list, &cur->elem, wakeup_first_with_priority, 0);//list:sleeplist(넣고자 하는 리스트), list_elem:cur thread list_elem
	thread_block();
	intr_set_level(prev_lv);
}

/* wake thread when waketime comes..*/
void thread_awake(int64_t cur_tick){
	struct thread *tmp;
	struct list_elem *e;

	//find thread's' that waketime already done, and awake them all.
	for(; e!=list_end(&sleep_list) && !list_empty(&sleep_list) ;){
		//가장 빨리 일어나야 하는 쓰레드를 가져온다.
		e = list_front(&sleep_list);
		tmp = list_entry(e,struct thread, elem);
		//일어날 시간이지났으면 일어나게 하고 sleep_list에서 제거한다.
		if(tmp->waketime <= cur_tick){
			list_pop_front(&sleep_list);
			thread_unblock(tmp);
		}
		//가장 시간이 빠른 것 조차 아직 시간이 덜 되었으면 그 이후의 것들은 볼 필요도 없다.
		else{
			break; //tick이 작은 순으로 이미 정렬되어있으므로,뒤의것들은 볼 필요조차 없다.
		}
	}
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore start_idle;
  sema_init (&start_idle, 0);
  thread_create ("idle", PRI_MIN, idle, &start_idle);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&start_idle);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. : interrupt가 올 수 있으므로 그냥 yield대신이거쓴단다.*/
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();

	/* Implementation for using Aging flag....*/
	if (thread_prior_aging == true)
		thread_aging();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  //printf("\n-----THREAD CREATE--------\ncurrent thread : %s,  child thread : %s,  allocated new tid : %d\n\n",running_thread()->name, t->name, t->tid);

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  /* Add to run queue. */
  ///

  //connect child thread with parent thread
  
  thread_unblock (t);

	//새로 생성하는 thread의 priority가 수행되고 있는 thread의 것보다 크다면? -> 새로 생성된 thread가 readylist의 맨 앞에 가도록 re-schedule해주어야 한다.
	//스케쥴링이 똑바로 되었다면 항상 수행되고 있는 thread의 priority가 가장 큰 값일 것이다.
	if (priority > thread_current()->priority){
		thread_yield();//단순히 schedule만호출할것이 아니라 cpu양도까지 하도록 한다.
	}	
  //reschedule_thread_by_priority();
  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  //list_push_back (&ready_list, &t->elem);
  	/* we should change here that considering Priority of thread*/
	list_insert_ordered(&ready_list, &t->elem, thread_comp_priority, NULL);
	t->status = THREAD_READY;
 // 	intr_set_level (old_level);

	//do not re-schedule in here!!! 
	//reschedule_thread_by_priority();

  	intr_set_level (old_level);
}
/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING; 
  schedule();
	NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  	if (cur != idle_thread) {//we have to modify considering priority{
		//list_push_back (&ready_list, &cur->elem);
		list_insert_ordered(&ready_list, &cur->elem, thread_comp_priority, 0);
	}
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

void reschedule_thread_by_priority(void){
	if (!list_empty (&ready_list) && thread_current()!=idle_thread && thread_current ()->priority < list_entry (list_front (&ready_list), struct thread, elem)->priority){
		thread_yield ();
	}
}


/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

bool is_idle_thread(struct thread *t){
	if (t == idle_thread) return true;
	else return false;
}


/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
	//int cur_priority = thread_current()->priority;
     thread_current ()->priority = new_priority;

	if (thread_mlfqs == true){
		return;
	}

	//if(cur_priority < new_priority && thread_current ()!= idle_thread)	
		thread_yield();
    // reschedule_thread_by_priority();	
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* compare priority of two threads */
bool thread_comp_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED){
	struct thread *t1, *t2;
	t1 = list_entry(a,struct thread, elem);
	t2 = list_entry(b,struct thread, elem);
	//true if t1 > t2
	return t1->priority > t2->priority;
}

//calculated repeatadly every 1 ticks..
void cal_recent_cpu_and_load_avg(){
	int ready_threads = list_size(&ready_list);
	struct list_elem *e;
	struct thread *travel;

	if (thread_current() != idle_thread)
		ready_threads +=1;

	//곱셈공식을 사용. a*c + b*c = c * (a + b) int/int를방지하기 위함.
	int tmp = R_mul_I(load_avg, 59);
	tmp = R_add_I(tmp, ready_threads);
	tmp = R_div_I(tmp, 60);
	load_avg = tmp;

//	printf("in cal_recent_cpu_sdfdf LOAD_AVG : %d\n",load_avg);
	//모든 thread들을 돌면서 recent_cpu값들을 갱신해준다. idle_thread뺴고.
	
	for(e = list_begin(&all_list) ; e!=list_end(&all_list) ; e=list_next(e)){
		travel = list_entry(e,struct thread, allelem);
		if(travel == idle_thread) continue;
		else{
			tmp = R_mul_I(load_avg, 2);
			tmp = R_div_R(tmp, R_add_I(tmp, 1));
			tmp = R_mul_R(tmp, travel->recent_cpu);
			tmp = R_add_I(tmp, travel->nice);
			travel->recent_cpu = tmp;
		}
	}
}

void cal_priority_using_aging(){
	struct list_elem *e;
	struct thread *travel;
	int tmp, niceT2;
	
	for (e = list_begin(&all_list);e!=list_end(&all_list);e=list_next(e)){
		travel = list_entry(e,struct thread, allelem);
		if (travel == idle_thread) continue;
		else{
			tmp = R_div_I(travel->recent_cpu, 4);
			niceT2 = travel->nice * 2;
			travel->priority = I_sub_R(R_add_I(0,PRI_MAX),tmp);
			travel->priority = R_sub_R(travel->priority,niceT2);
			
		    if (travel->priority > PRI_MAX) {
			      travel->priority = PRI_MAX;
    		}
    		else if (travel->priority < PRI_MIN) {
      			travel->priority = PRI_MIN;
    		}
		}
	}
	//re-sort ready list by updated priority
	list_sort(&ready_list,thread_comp_priority,NULL);
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) 
{
  /* Not yet implemented. */
	thread_current()->nice = nice;
	cal_recent_cpu_and_load_avg();
	cal_priority_using_aging();
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  /* Not yet implemented. */
  return thread_current()->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  /* Not yet implemented. */
 return (R_mul_I(load_avg,100)/(1<<14));
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  /* Not yet implemented. */
  return (R_mul_I(thread_current()->recent_cpu,100)/(1<<14));
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
// struct thread *cur;
// cur = thread_current();
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->magic = THREAD_MAGIC;
  list_push_back (&all_list, &t->allelem);
  t->parent = running_thread();

  /* proj3. nice값과 recent_cpu값을 현재 running thread로부터 인계받자.*/
	t->recent_cpu = running_thread()->recent_cpu;
    t->nice = running_thread()->recent_cpu;
#ifdef USERPROG
	int i;//initialize file structures to NULL
	for(i=0; i<128; i++){
		t->files[i] = NULL;
	}
	
	//initializing semaphores for threads(process)
  	sema_init(&(t->sema_lock), 0);
  	sema_init(&(t->sema_mem), 0);
	/*semaphore to stay alive parent until child is successfully loaded*/
	sema_init(&(t->sema_load), 0);
  //ls_child
    
    /*successfull exec*/
    t->exec_success = true; 

  list_init(&(t->ls_child));
  list_push_back(&(running_thread()->ls_child),&(t->ls_child_elem));
  // list_push_back(&(t->ls_child), &(t->ls_child_elem));
#endif
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
