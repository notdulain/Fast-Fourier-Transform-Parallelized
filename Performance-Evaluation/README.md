# FFT Performance Evaluation

Each configuration is tested once using `1,048,576` samples.

## Files

- `OpenMP/results.txt`: OpenMP execution times and speedups.
- `MPI/results.txt`: MPI execution times and speedups.
- `CUDA/results.txt`: CUDA results or an unavailable message.
- `Comparative/results.txt`: comparison and simple analysis.
- Each `graphs/` folder contains the required graphs.
- `evaluate.py`: runs the tests and creates all result files and graphs.

## Run

Install Matplotlib:

```sh
python3 -m pip install -r Performance-Evaluation/requirements.txt
```

Run the evaluation:

```sh
python3 Performance-Evaluation/evaluate.py
```

CUDA results are generated only on a computer with an NVIDIA CUDA GPU and
the `nvcc` compiler.
