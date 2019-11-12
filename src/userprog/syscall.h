#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

typedef int pid_t;

void syscall_init(void);

/*prototypes of syscall funcions*/
void halt(void);
void exit(int status);
pid_t exec(const char *cmd_line);
int wait(pid_t pid);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);

/*2_2 added userprog*/
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);


//user defined system call functions;
int fibonacci(int n);
int sum_of_four_int(int a, int b, int c, int d);


#endif /* userprog/syscall.h */
