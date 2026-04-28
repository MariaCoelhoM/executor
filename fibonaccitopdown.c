#include <stdio.h>

// Definimos um tamanho máximo para o array de memória
#define MAX 100

// Array global para armazenar os resultados (o "memo")
long long memo[MAX];

long long fib(int n) {
    // 1 se memo[n] != -1
    if (memo[n] != -1) {
        // 2 retorne memo[n]
        return memo[n];
    }
    
    // 3 senão se n = 0 ou n = 1
    if (n == 0 || n == 1) {
        // 4 memo[n] = n
        memo[n] = n;
    } 
    // 5 senão
    else {
        // 6 memo[n] = fib(n - 1) + fib(n - 2)
        memo[n] = fib(n - 1) + fib(n - 2);
    }
    
    // 7 retorne memo[n]
    return memo[n];
}

int main() {
    int n = 50; // Agora podemos calcular números maiores sem travar!

    // Inicializa o memo com -1
    for (int i = 0; i < MAX; i++) {
        memo[i] = -1;
    }

    printf("Fibonacci de %d: %lld\n", n, fib(n));

    return 0;
}