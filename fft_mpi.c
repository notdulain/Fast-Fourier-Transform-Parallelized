#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <mpi.h>

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
 * MPI-parallel FFT.
 *
 * Each process holds local_n elements (a contiguous chunk of the
 * bit-reversed array). The FFT butterfly stages are split into:
 *
 *   LOCAL stages  — butterfly pairs are within the same process
 *   GLOBAL stages — butterfly pairs span two different processes
 */
 void fft_mpi(double *local_real, double *local_imag,
    int n, int local_n, int rank, int size)
{
/* Buffer for receiving partner's data during global stages */
double *recv_real = (double *)malloc(local_n * sizeof(double));
double *recv_imag = (double *)malloc(local_n * sizeof(double));

/*
* ---- LOCAL STAGES ----
* When group size (len) <= local_n, every butterfly pair lives
* on the same process. No communication needed.
* Same logic as OpenMP version — flatten into independent butterflies.
*/
for (int len = 2; len <= local_n; len <<= 1) {
int half = len / 2;

for (int k = 0; k < local_n / 2; k++) {
   int group_start = (k / half) * len;
   int j = k % half;

   double angle = -2.0 * PI * j / len;
   double wr = cos(angle);
   double wi = sin(angle);

   int u = group_start + j;
   int v = group_start + j + half;

   double tr = wr * local_real[v] - wi * local_imag[v];
   double ti = wr * local_imag[v] + wi * local_real[v];

   local_real[v] = local_real[u] - tr;
   local_imag[v] = local_imag[u] - ti;
   local_real[u] += tr;
   local_imag[u] += ti;
}
}

/*
* ---- GLOBAL STAGES ----
* When group size (len) > local_n, each butterfly connects an
* element on THIS process with an element on a PARTNER process.
*
* Partner is found by XOR-ing the rank with (half / local_n).
* If our rank < partner, we hold the "upper" (u) elements.
* If our rank > partner, we hold the "lower" (v) elements.
*
* Each process sends its entire local array to partner,
* receives partner's array, then computes its half of all butterflies.
*/
for (int len = 2 * local_n; len <= n; len <<= 1) {
int half = len / 2;
int partner = rank ^ (half / local_n);

/* Exchange local data with partner process */
MPI_Sendrecv(local_real, local_n, MPI_DOUBLE, partner, 0,
            recv_real,  local_n, MPI_DOUBLE, partner, 0,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE);
MPI_Sendrecv(local_imag, local_n, MPI_DOUBLE, partner, 1,
            recv_imag,  local_n, MPI_DOUBLE, partner, 1,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE);

int is_lower = (rank < partner);

for (int j = 0; j < local_n; j++) {
   /*
    * Compute twiddle factor from global position.
    * tw_idx = position within the lower half of the group.
    */
   int global_idx = rank * local_n + j;
   int tw_idx = global_idx % half;

   double angle = -2.0 * PI * tw_idx / len;
   double wr = cos(angle);
   double wi = sin(angle);

   double ur, ui, vr, vi;

   if (is_lower) {
       /* We have u (lower half), partner sent v (upper half) */
       ur = local_real[j];  ui = local_imag[j];
       vr = recv_real[j];   vi = recv_imag[j];
   } else {
       /* We have v (upper half), partner sent u (lower half) */
       ur = recv_real[j];   ui = recv_imag[j];
       vr = local_real[j];  vi = local_imag[j];
   }

   /* Butterfly operation */
   double tr = wr * vr - wi * vi;
   double ti = wr * vi + wi * vr;

   if (is_lower) {
       local_real[j] = ur + tr;   /* keep u + tw*v */
       local_imag[j] = ui + ti;
   } else {
       local_real[j] = ur - tr;   /* keep u - tw*v */
       local_imag[j] = ui - ti;
   }
}
}

free(recv_real);
free(recv_imag);
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int n = 1 << 20;
    int local_n = n / size;

    double *real = NULL, *imag = NULL;

    /*
     * Root process: generate the signal and do bit-reversal
     * on the full array BEFORE distributing.
     */
    if (rank == 0) {
        real = (double *)malloc(n * sizeof(double));
        imag = (double *)malloc(n * sizeof(double));

        for (int i = 0; i < n; i++) {
            real[i] = sin(2.0 * PI * 440.0 * i / n)
                    + 0.5 * sin(2.0 * PI * 880.0 * i / n);
            imag[i] = 0.0;
        }

        bit_reverse(real, imag, n);
    }

    /* Each process allocates space for its chunk */
    double *local_real = (double *)malloc(local_n * sizeof(double));
    double *local_imag = (double *)malloc(local_n * sizeof(double));

    /* Distribute the bit-reversed array evenly across all processes */
    MPI_Scatter(real, local_n, MPI_DOUBLE,
                local_real, local_n, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Scatter(imag, local_n, MPI_DOUBLE,
                local_imag, local_n, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    /* Time only the FFT computation */
    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    fft_mpi(local_real, local_imag, n, local_n, rank, size);

    MPI_Barrier(MPI_COMM_WORLD);
    double end = MPI_Wtime();

    /* Gather results back to root */
    MPI_Gather(local_real, local_n, MPI_DOUBLE,
               real, local_n, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Gather(local_imag, local_n, MPI_DOUBLE,
               imag, local_n, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("FFT of %d samples completed in %.4f seconds.\n", n, end - start);

        printf("First 5 output magnitudes:\n");
        for (int i = 0; i < 5; i++) {
            double mag = sqrt(real[i]*real[i] + imag[i]*imag[i]);
            printf("  X[%d] = %.4f\n", i, mag);
        }
        printf("\nPeak magnitudes:\n");
        printf("  X[440] = %.4f\n",
               sqrt(real[440]*real[440] + imag[440]*imag[440]));
        printf("  X[880] = %.4f\n",
               sqrt(real[880]*real[880] + imag[880]*imag[880]));

        free(real);
        free(imag);
    }

    free(local_real);
    free(local_imag);
    MPI_Finalize();
    return 0;
}
