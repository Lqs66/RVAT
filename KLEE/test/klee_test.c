#include "klee/klee.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
uint8_t heap[16];

struct A{
 int a;
 int b;
 int* p;
};
struct A a;
bool symbolic_b;
int symbolic_c;
int* ptr;
int main(){
    // int symbolic_c;
    klee_make_symbolic(&symbolic_b, sizeof(bool), "symbolic_b");

    klee_make_symbolic(&symbolic_c, sizeof(int), "symbolic_c");

    klee_make_symbolic(heap, 16, "heap");

    // int c = symbolic_c;
    klee_make_symbolic(&a, 16, "symbolic_a");

    ptr = (int*)((uint8_t*)&a + 4);

    *(void**)((uint8_t*)&a + 8) = (void*)((uint8_t*)&a + 4);

    *(uint8_t **)heap = (uint8_t*)&a + 4;
    *(uint8_t **)(heap + 8) = (uint8_t*)(heap + 8);

    // if (a.a >= 1 && a.a <= 10){
    //     printf("a.a >= 1 && a.a <= 10\n");
    // }else if(a.a < 1){
    //     printf("a.a < 1\n");
    // } else{
    //     printf("a.a not in [1, 10]\n");
    // }
    // if(a.p == &a){
    //     printf("a.p == &a\n");
    // }else{
    //     printf("a.p != &a\n");
    // }
    // printf("%p\n", a.p);
}