#include <klib.h>

int main()
{
    asm volatile(
    "mul    a1,a2,a3 \n"
    "mul    a2,a4,a2 \n"
    "mul    a4,a4,a5 \n"
    "mul    a5,a5,a3 \n"
    "add    a3,a5,a4 \n"
    );
    asm volatile(
    "nop \n"
    "mul    a1,a2,a3 \n"
    "mul    a2,a4,a2 \n"
    "mul    a4,a4,a5 \n"
    "mul    a5,a5,a3 \n"
    "add    a3,a5,a4 \n"
    );
    asm volatile(
    "nop \n"
    "nop \n"
    "mul    a1,a2,a3 \n"
    "mul    a2,a4,a2 \n"
    "mul    a4,a4,a5 \n"
    "mul    a5,a5,a3 \n"
    "add    a3,a5,a4 \n"
    );
    asm volatile(
    "nop \n"
    "nop \n"
    "nop \n"
    "mul    a1,a2,a3 \n"
    "mul    a2,a4,a2 \n"
    "mul    a4,a4,a5 \n"
    "mul    a5,a5,a3 \n"
    "add    a3,a5,a4 \n"
    );
    asm volatile(
    "nop \n"
    "nop \n"
    "nop \n"
    "nop \n"
    "mul    a1,a2,a3 \n"
    "mul    a2,a4,a2 \n"
    "mul    a4,a4,a5 \n"
    "mul    a5,a5,a3 \n"
    "add    a3,a5,a4 \n"
    );
    printf("Hello, XiangShan!\n");
    return 0;
}
