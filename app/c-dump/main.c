#include <stdio.h>
#include <stdlib.h>

int febonacci(int n)
{
    if (n <= 1)
        return n;
    else
        return febonacci(n - 1) + febonacci(n - 2);
}

int main()
{
    int n[50];

    for (int i = 0; i < 10; i++)
    {
        n[i] = febonacci(i);
    }

    register int a7 asm("a7") = 1025; // SYS_dump

    asm volatile("ecall"
                 :
                 : "r"(a7)
                 : "memory");

    for (int i = 0; i < 10; i++)
    {
        printf("%d ", n[i]);
    }
    return 0;
}