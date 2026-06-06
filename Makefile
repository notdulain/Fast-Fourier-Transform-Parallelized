CC := clang
MPICC := mpicc
NVCC := nvcc

CFLAGS := -O3
OPENMP_FLAGS := -Xpreprocessor -fopenmp \
	-I/opt/homebrew/opt/libomp/include \
	-L/opt/homebrew/opt/libomp/lib -lomp

MPI_PROCESSES ?= 4
OMP_THREADS ?= 4
CUDA_BLOCK_SIZE ?= 256

SERIAL_SRC := Serial/fft_serial.c
SERIAL_BIN := Serial/fft_serial
OPENMP_SRC := OpenMP/fft_omp.c
OPENMP_BIN := OpenMP/fft_omp
MPI_SRC := MPI/fft_mpi.c
MPI_BIN := MPI/fft_mpi
CUDA_SRC := CUDA/fft_cuda.cu
CUDA_BIN := CUDA/fft_cuda

.PHONY: all serial openmp mpi cuda run run-serial run-openmp run-mpi run-cuda evaluate clean

all: serial openmp mpi

serial: $(SERIAL_BIN)

openmp: $(OPENMP_BIN)

mpi: $(MPI_BIN)

cuda: $(CUDA_BIN)

$(SERIAL_BIN): $(SERIAL_SRC)
	$(CC) $(CFLAGS) $< -o $@ -lm

$(OPENMP_BIN): $(OPENMP_SRC)
	$(CC) $(CFLAGS) $(OPENMP_FLAGS) $< -o $@ -lm

$(MPI_BIN): $(MPI_SRC)
	$(MPICC) $(CFLAGS) $< -o $@ -lm

$(CUDA_BIN): $(CUDA_SRC)
	$(NVCC) $(CFLAGS) $< -o $@

run: run-serial run-openmp run-mpi

run-serial: serial
	./$(SERIAL_BIN)

run-openmp: openmp
	OMP_NUM_THREADS=$(OMP_THREADS) ./$(OPENMP_BIN)

run-mpi: mpi
	mpirun --oversubscribe -np $(MPI_PROCESSES) ./$(MPI_BIN)

run-cuda: cuda
	./$(CUDA_BIN) $(CUDA_BLOCK_SIZE)

evaluate:
	python3 Performance-Evaluation/evaluate.py

clean:
	rm -f $(SERIAL_BIN) $(OPENMP_BIN) $(MPI_BIN) $(CUDA_BIN)
