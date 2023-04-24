#include <stdio.h>
int main()
{
    int arr[100];
    int sum = 0;
    for (int i = 0; i < 100; i += 2)
    {
        int j = i * 3 - 2;
        sum += j;
        arr[i] = sum;
    }
    printf("%d", arr[34]);
    return 0;
}