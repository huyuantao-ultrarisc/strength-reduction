#include <stdio.h>
#include <time.h>

int main()
{
    int s = clock();
    int sum = 0;
    for (int i = 0; i < 1000000000; i++)
    {
        int j = i * 3 - 2;
        sum += j;
    }
    int t = clock();
    printf("%d, %lf\n",sum,(double)(t-s)/CLOCKS_PER_SEC);
    return 0;
}