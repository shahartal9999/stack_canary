#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bof(const char * str_buf)
{
    int i;
    char buf[10];
    strcpy(buf, str_buf);
    return 1;
}

int main(int argc, char **argv)
{
    int ret = 0;
    if (argc < 2){
        printf("Missing input\n");
        return 0;
    }

    printf("rand() = %d", rand());
    printf("null is %d", malloc(0));
    printf("before bof\n");
    ret = bof(argv[1]);
    printf("bof() = %d\n", ret);
    printf("after bof\n");
    return 0;
}
