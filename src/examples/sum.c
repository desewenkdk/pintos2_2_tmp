#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include "../lib/user/syscall.h"

int main(int argc, char *argv[]){
    int fibo, sum;
    if (argc < 5){
        printf("lack of function arguments\n");
        return -1;
    }

    fibo = fibonacci(atoi(argv[1]));
    sum = sum_of_four_int(atoi(argv[1]),atoi(argv[2]),atoi(argv[3]),atoi(argv[4]));
    printf("%d %d\n",fibo, sum);
    return 0;
}
