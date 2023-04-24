#include <stdio.h>

void put_hex(int x)
{
    if (x < 10)
        printf("%d", x%10);
    else
        printf("%c", 'a' + x - 10);
}

int main()
{
    FILE *file = fopen("./main", "wr");

    for (int i = 0; i < 7; i++)
    {
        unsigned char ch = ((unsigned char *)file)[i];
        unsigned char low = ch % 16;
        unsigned char high = ch / 16;
        put_hex(low),put_hex(high);
        putchar(' ');
    }

    fclose(file);
}