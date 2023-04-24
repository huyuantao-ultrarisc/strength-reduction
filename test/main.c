#include <stdio.h>
#include <time.h>

int main()
{
    int sum = 0;
    int s = clock();
    
    for (int i = 0; i < 1000000000; i += 2)
    {
        int j = i * 3 - 2;
        sum += j;
    }
    int e = clock();
    printf("%d\n",sum);
    printf("%lf\n",(double)(e-s)/CLOCKS_PER_SEC);
    return 0;
}