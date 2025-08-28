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
    for (int i = 0; i < 50; i++)
    {
        printf("%d\n", febonacci(i));
    }
    return 0;
}