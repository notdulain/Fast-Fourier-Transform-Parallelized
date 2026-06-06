#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cuda_runtime.h>

#define PI 3.14159265358979323846

/*
 * Kernel 1: Bit-reversal permutation
 * Each thread handles one index — computes its bit-reversed
 * partner and swaps if needed. Only swaps when i < j to
 * avoid two threads swapping the same pair twice.
 */
__global__ void bit_reverse_kernel(double *real, double *imag,
                                   int n, int log2n)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;

    /* Compute bit-reversed index of i */
    int j = 0;
    int temp = i;
    for (int b = 0; b < log2n; b++) {
        j = (j << 1) | (temp & 1);
        temp >>= 1;
    }

    /* Only swap when i < j so each pair is swapped exactly once */
    if (i < j) {
        double tmp;
        tmp = real[i]; real[i] = real[j]; real[j] = tmp;
        tmp = imag[i]; imag[i] = imag[j]; imag[j] = tmp;
    }
}

/*
 * Kernel 2: One butterfly stage
 * Each thread performs one butterfly operation.
 * Total threads needed per stage = n/2.
 * Launched once per stage from the host loop.
 */
__global__ void butterfly_kernel(double *real, double *imag,
                                 int n, int len)
{
    int k = blockIdx.x * blockDim.x + threadIdx.x;
    if (k >= n / 2) return;

    int half = len / 2;
    int group_start = (k / half) * len;
    int j = k % half;

    /* Direct twiddle factor computation — same idea as OpenMP version */
    double angle = -2.0 * PI * j / len;
    double wr = cos(angle);
    double wi = sin(angle);

    int u = group_start + j;
    int v = group_start + j + half;

    /* Butterfly — identical math to serial */
    double tr = wr * real[v] - wi * imag[v];
    double ti = wr * imag[v] + wi * real[v];

    real[v] = real[u] - tr;
    imag[v] = imag[u] - ti;
    real[u] = real[u] + tr;
    imag[u] = imag[u] + ti;
}

int main(int argc, char *argv[]) {
    int n = 1 << 20;
    int log2n = 20;
    int blockSize = 256;

    if (argc > 1) {
        blockSize = atoi(argv[1]);
    }
    if (blockSize <= 0 || blockSize > 1024) {
        fprintf(stderr, "Block size must be between 1 and 1024.\n");
        return 1;
    }

    /* ---------- Host: generate signal ---------- */
    double *h_real = (double *)malloc(n * sizeof(double));
    double *h_imag = (double *)malloc(n * sizeof(double));

    for (int i = 0; i < n; i++) {
        h_real[i] = sin(2.0 * PI * 440.0 * i / n)
                  + 0.5 * sin(2.0 * PI * 880.0 * i / n);
        h_imag[i] = 0.0;
    }

    /* ---------- Allocate device memory ---------- */
    double *d_real, *d_imag;
    cudaMalloc(&d_real, n * sizeof(double));
    cudaMalloc(&d_imag, n * sizeof(double));

    /* ---------- Copy signal to GPU ---------- */
    cudaMemcpy(d_real, h_real, n * sizeof(double), cudaMemcpyHostToDevice);
    cudaMemcpy(d_imag, h_imag, n * sizeof(double), cudaMemcpyHostToDevice);

    /* ---------- Kernel launch configuration ---------- */
    int gridSize_full = (n + blockSize - 1) / blockSize;       /* for bit-reverse: n threads */
    int gridSize_half = (n / 2 + blockSize - 1) / blockSize;   /* for butterfly: n/2 threads */

    /* ---------- Time using CUDA events ---------- */
    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventRecord(start);

    /* Step 1: bit-reversal on GPU */
    bit_reverse_kernel<<<gridSize_full, blockSize>>>(d_real, d_imag, n, log2n);

    /* Step 2: butterfly stages — one kernel launch per stage
     * 20 stages for n = 2^20, each launching n/2 = 524288 threads */
    for (int len = 2; len <= n; len <<= 1) {
        butterfly_kernel<<<gridSize_half, blockSize>>>(d_real, d_imag, n, len);
    }

    cudaEventRecord(stop);
    cudaEventSynchronize(stop);

    float milliseconds = 0;
    cudaEventElapsedTime(&milliseconds, start, stop);

    /* ---------- Copy results back to host ---------- */
    cudaMemcpy(h_real, d_real, n * sizeof(double), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_imag, d_imag, n * sizeof(double), cudaMemcpyDeviceToHost);

    /* ---------- Print results ---------- */
    printf("Implementation: CUDA\n");
    printf("CUDA block size: %d\n", blockSize);
    printf("CUDA full grid blocks: %d\n", gridSize_full);
    printf("CUDA butterfly grid blocks: %d\n", gridSize_half);
    printf("FFT of %d samples completed in %.6f seconds.\n", n, milliseconds / 1000.0);

    printf("First 5 output magnitudes:\n");
    for (int i = 0; i < 5; i++) {
        double mag = sqrt(h_real[i]*h_real[i] + h_imag[i]*h_imag[i]);
        printf("  X[%d] = %.4f\n", i, mag);
    }
    printf("\nPeak magnitudes:\n");
    printf("  X[440] = %.4f\n",
           sqrt(h_real[440]*h_real[440] + h_imag[440]*h_imag[440]));
    printf("  X[880] = %.4f\n",
           sqrt(h_real[880]*h_real[880] + h_imag[880]*h_imag[880]));

    /* ---------- Cleanup ---------- */
    cudaFree(d_real);
    cudaFree(d_imag);
    cudaEventDestroy(start);
    cudaEventDestroy(stop);
    free(h_real);
    free(h_imag);
    return 0;
}
