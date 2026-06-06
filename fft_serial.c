#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define PI 3.14159265358979323846

/*
 * Bit-reversal permutation.
 * Reorders the input array so that the iterative FFT
 * processes elements in the correct order.
 */
void bit_reverse(double *real, double *imag, int n) {
    int j = 0;
    for (int i = 1; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double tmp;
            tmp = real[i]; real[i] = real[j]; real[j] = tmp;
            tmp = imag[i]; imag[i] = imag[j]; imag[j] = tmp;
        }
    }
}

/*
 * Iterative Cooley-Tukey Radix-2 FFT.
 * Input:  real[], imag[] arrays of length n (n must be a power of 2)
 * Output: FFT result written back into real[] and imag[]
 */
void fft(double *real, double *imag, int n) {
    bit_reverse(real, imag, n);

    /* Iteratively combine sub-FFTs of increasing size */
    for (int len = 2; len <= n; len <<= 1) {
        double angle = -2.0 * PI / len;
        double wr_step = cos(angle);
        double wi_step = sin(angle);

        for (int i = 0; i < n; i += len) {
            double wr = 1.0, wi = 0.0;  /* twiddle factor, starts at 1 */

            for (int j = 0; j < len / 2; j++) {
                int u = i + j;
                int v = i + j + len / 2;

                /* Butterfly operation */
                double tr = wr * real[v] - wi * imag[v];
                double ti = wr * imag[v] + wi * real[v];

                real[v] = real[u] - tr;
                imag[v] = imag[u] - ti;
                real[u] = real[u] + tr;
                imag[u] = imag[u] + ti;

                /* Advance twiddle factor */
                double new_wr = wr * wr_step - wi * wi_step;
                wi = wr * wi_step + wi * wr_step;
                wr = new_wr;
            }
        }
    }
}

int main() {
    int n = 1 << 20;  /* 2^20 = 1,048,576 samples */

    double *real = (double *)malloc(n * sizeof(double));
    double *imag = (double *)malloc(n * sizeof(double));

    /* Initialize with a simple test signal (sum of two sine waves) */
    for (int i = 0; i < n; i++) {
        real[i] = sin(2.0 * PI * 440.0 * i / n)
                + 0.5 * sin(2.0 * PI * 880.0 * i / n);
        imag[i] = 0.0;
    }

    clock_t start = clock();
    fft(real, imag, n);
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("FFT of %d samples completed in %.4f seconds.\n", n, elapsed);

    /* Print first 5 magnitudes as a basic sanity check */
    printf("First 5 output magnitudes:\n");
    for (int i = 0; i < 5; i++) {
        double mag = sqrt(real[i]*real[i] + imag[i]*imag[i]);
        printf("  X[%d] = %.4f\n", i, mag);
    }

    free(real);
    free(imag);
    return 0;
}
