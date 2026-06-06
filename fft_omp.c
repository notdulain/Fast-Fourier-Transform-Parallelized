#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <omp.h>

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

    /*
         * Instead of looping over groups (i) then within each group (j),
         * we flatten it into a single loop over ALL butterfly indices.
         * This way every iteration is independent — perfect for OpenMP.
         *
         * Total butterflies per pass = n/2
         * Each butterfly index 'k' maps to:
         *   - which group it belongs to (group_start)
         *   - which position within that group (j)
         */
         int half = n / 2;

         #pragma omp parallel for schedule(static)
         for (int k = 0; k < n / 2; k++) {
             /* Figure out which group and position this butterfly belongs to */
             int group_start = (k / half) * n;
             int j = k % half;
 
             /* Compute twiddle factor directly from j instead of accumulating.
              * This avoids the sequential dependency of advancing wr/wi. */
             double angle_j = -2.0 * PI * j / n;
             double wr = cos(angle_j);
             double wi = sin(angle_j);
 
             int u = group_start + j;
             int v = group_start + j + half;
 
             /* Butterfly operation — unchanged from serial */
             double tr = wr * real[v] - wi * imag[v];
             double ti = wr * imag[v] + wi * real[v];
 
             real[v] = real[u] - tr;
             imag[v] = imag[u] - ti;
             real[u] = real[u] + tr;
             imag[u] = imag[u] + ti;
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

    double start = omp_get_wtime();
    fft(real, imag, n);
    double end = omp_get_wtime();

    double elapsed = end - start;
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
