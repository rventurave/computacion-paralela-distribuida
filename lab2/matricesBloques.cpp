#include <iostream>
#include <vector>
#include <chrono>
#include <random>

using namespace std;

vector<vector<double>> generarMatriz(int n) {
    vector<vector<double>> M(n, vector<double>(n));
    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<> dis(0.0, 1.0);

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            M[i][j] = dis(gen);
        }
    }
    return M;
}

void multiplicacionClasica(const vector<vector<double>>& A, 
                           const vector<vector<double>>& B, 
                           vector<vector<double>>& C, int N) {
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            C[i][j] = 0.0;
            for (int k = 0; k < N; ++k) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

void multiplicacionBloques(const vector<vector<double>>& A, 
                           const vector<vector<double>>& B, 
                           vector<vector<double>>& C, int N, int blockSize) {
    // Inicializar matriz C en 0
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            C[i][j] = 0.0;

    // Tres bucles externos: iteran sobre los bloques
    for (int ii = 0; ii < N; ii += blockSize) {
        for (int jj = 0; jj < N; jj += blockSize) {
            for (int kk = 0; kk < N; kk += blockSize) {
                
                // Tres bucles internos: multiplicación estándar dentro del bloque
                for (int i = ii; i < min(ii + blockSize, N); ++i) {
                    for (int j = jj; j < min(jj + blockSize, N); ++j) {
                        double suma = 0.0;
                        for (int k = kk; k < min(kk + blockSize, N); ++k) {
                            suma += A[i][k] * B[k][j];
                        }
                        C[i][j] += suma;
                    }
                }

            }
        }
    }
}

int main() {
    int N = 1024;        // Tamaño de la matriz N x N
    int blockSize = 64;  // Tamaño del bloque B x B

    cout << "Generando matrices de " << N << "x" << N << "..." << endl;
    auto A = generarMatriz(N);
    auto B = generarMatriz(N);
    vector<vector<double>> C(N, vector<double>(N, 0.0));

    // Evaluar Clásica
    auto start = chrono::high_resolution_clock::now();
    multiplicacionClasica(A, B, C, N);
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> t_clasico = end - start;
    cout << "Tiempo Clásico: " << t_clasico.count() << " segundos." << endl;

    // Evaluar Bloques
    start = chrono::high_resolution_clock::now();
    multiplicacionBloques(A, B, C, N, blockSize);
    end = chrono::high_resolution_clock::now();
    chrono::duration<double> t_bloques = end - start;
    cout << "Tiempo por Bloques (" << blockSize << "x" << blockSize << "): " 
         << t_bloques.count() << " segundos." << endl;

    return 0;
}
