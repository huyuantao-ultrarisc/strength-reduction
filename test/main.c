#include <stdio.h>
int main()
{
    int sum = 0;
    for (int i = 1; i < 100; i += 2)
    {
        int j = i * 3 - 2;
        sum += j;
    }
    printf("%d",sum);
    // ...
}