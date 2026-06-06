# Fast Fourier Transform Parallelized

This project implements the iterative Cooley-Tukey radix-2 Fast Fourier
Transform (FFT) using four approaches:

- Serial C
- OpenMP shared-memory parallelism
- MPI distributed-memory parallelism
- CUDA GPU parallelism

Each implementation processes `2^20 = 1,048,576` samples containing two sine
waves at frequency bins 440 and 880. The parallel implementations are evaluated
using different thread, process, and CUDA block-size configurations.

## Folder Structure

```text
.
├── Serial/
│   └── fft_serial.c
├── OpenMP/
│   └── fft_omp.c
├── MPI/
│   └── fft_mpi.c
├── CUDA/
│   └── fft_cuda.cu
├── Performance-Evaluation/
│   ├── OpenMP/
│   ├── MPI/
│   ├── CUDA/
│   │   └── cuda_colab_benchmark.ipynb
│   ├── Comparative/
│   ├── evaluate.py
│   └── requirements.txt
├── Makefile
└── README.md
```

The performance-evaluation folders contain the measured results and generated
graphs for each implementation.

## Requirements

For Serial, OpenMP, and MPI:

- `make`
- Apple Clang
- OpenMP runtime (`libomp`)
- Open MPI (`mpicc` and `mpirun`)
- Python 3 and Matplotlib for generating evaluation graphs

Install the Python graph dependency:

```sh
python3 -m pip install -r Performance-Evaluation/requirements.txt
```

## Build Using Make

Build the Serial, OpenMP, and MPI implementations:

```sh
make all
```

Build an individual implementation:

```sh
make serial
make openmp
make mpi
make cuda
```

The CUDA target requires an NVIDIA GPU and the `nvcc` compiler.

## Run Using Make

Run the Serial implementation:

```sh
make run-serial
```

Run OpenMP with a selected number of threads:

```sh
make run-openmp OMP_THREADS=8
```

Run MPI with a selected number of processes:

```sh
make run-mpi MPI_PROCESSES=8
```

Run CUDA with a selected number of threads per block:

```sh
make run-cuda CUDA_BLOCK_SIZE=512
```

Run Serial, OpenMP, and MPI using the default configurations:

```sh
make run
```

Remove compiled executables:

```sh
make clean
```

## Performance Evaluation

Run the local performance evaluation:

```sh
make evaluate
```

This evaluates:

- OpenMP with `1, 2, 4, 8, 16` threads
- MPI with `1, 2, 4, 8, 16` processes
- CUDA with `32, 64, 128, 256, 512, 1024` threads per block when CUDA is
  available

Each configuration is run once. The script creates `results.txt` files and the
required execution-time and speedup graphs.

## Running CUDA in Google Colab

CUDA cannot run on computers without an NVIDIA GPU. Google Colab can be used
to compile and benchmark the CUDA implementation.

1. Open [cuda_colab_benchmark.ipynb](Performance-Evaluation/CUDA/cuda_colab_benchmark.ipynb)
   in Google Colab.
2. Select **Runtime > Change runtime type**.
3. Select a GPU hardware accelerator.
4. Run the notebook cells in order.
5. When requested, upload [fft_cuda.cu](CUDA/fft_cuda.cu).
6. The notebook compiles the CUDA source using `nvcc`.
7. It tests each CUDA block size once and generates:
   - `results.txt`
   - `execution_time.png`
   - `speedup.png`
8. Download the generated files and place them in:

```text
Performance-Evaluation/CUDA/
Performance-Evaluation/CUDA/graphs/
```

After adding the CUDA results, the comparative results and graphs can be
updated using the evaluation script.
