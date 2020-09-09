#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>

# define SIZE 30
//Gustavo Paez Villalobos A01039751
// Rafa Mtz
// Christopher Parga A00818942
// Grupo 3


static volatile sig_atomic_t keep_running = 1;

static void sig_handler(int _){
    puts("Handler.");
    keep_running = 1;
}

int main(void){
    signal(SIGINT, sig_handler);

    while (keep_running)
        puts("Corriendo...");

    
    return 0;
}