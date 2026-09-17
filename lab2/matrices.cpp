#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void inicializar(double *A, double *B, double *C, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            A[i * n + j] = 1.0;
            B[i * n + j] = 1.0;
            C[i * n + j] = 0.0;
        }
    }
}

void multiplicar(double *A, double *B, double *C, int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            for (int k = 0; k < n; k++)
                C[i * n + j] += A[i * n + k] * B[k * n + j];
}

int main() {
    int tamanos[] = {64, 256, 512, 1024};

    for (int t = 0; t < 4; t++) {
        int n = tamanos[t];

        double *A = new double[n * n];
        double *B = new double[n * n];
        double *C = new double[n * n];

        inicializar(A, B, C, n);

        clock_t inicio = clock();

        multiplicar(A, B, C, n);

        clock_t fin = clock();

        double tiempo = (double)(fin - inicio) / CLOCKS_PER_SEC;

        printf("N = %d | Tiempo: %.5f s\n", n, tiempo);

        delete[] A;
        delete[] B;
        delete[] C;
    }

    return 0;
}
