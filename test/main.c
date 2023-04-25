#include <stdio.h>
#include <time.h>

int main()
{
    int sum = 0;
    for (int i = 0; i < 1000; i += 2)
    {
        int j = i * 3 - 2;
        sum += j;
    }
    printf("%d",sum);
    return 0;
}