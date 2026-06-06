#!/usr/bin/env python3
"""Run each FFT configuration once and generate the required graphs."""

import os
import re
import shutil
import subprocess
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


EVAL_DIR = Path(__file__).resolve().parent
ROOT_DIR = EVAL_DIR.parent
SERIAL_DIR = ROOT_DIR / "Serial"
OPENMP_DIR = ROOT_DIR / "OpenMP"
MPI_DIR = ROOT_DIR / "MPI"
CUDA_DIR = ROOT_DIR / "CUDA"
TIME_PATTERN = re.compile(r"completed in ([0-9.]+) seconds")
CONFIGURATIONS = [1, 2, 4, 8, 16]
CUDA_BLOCK_SIZES = [32, 64, 128, 256, 512, 1024]


def command(args, env=None):
    result = subprocess.run(
        args, cwd=ROOT_DIR, env=env, capture_output=True, text=True
    )
    if result.returncode != 0:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout


def build():
    command(
        [
            "clang",
            "-O3",
            str(SERIAL_DIR / "fft_serial.c"),
            "-o",
            str(SERIAL_DIR / "fft_serial"),
            "-lm",
        ]
    )
    command(
        [
            "clang",
            "-O3",
            "-Xpreprocessor",
            "-fopenmp",
            "-I/opt/homebrew/opt/libomp/include",
            "-L/opt/homebrew/opt/libomp/lib",
            "-lomp",
            str(OPENMP_DIR / "fft_omp.c"),
            "-o",
            str(OPENMP_DIR / "fft_omp"),
            "-lm",
        ]
    )
    command(
        [
            "mpicc",
            "-O3",
            str(MPI_DIR / "fft_mpi.c"),
            "-o",
            str(MPI_DIR / "fft_mpi"),
            "-lm",
        ]
    )
    if shutil.which("nvcc"):
        command(
            [
                "nvcc",
                "-O3",
                str(CUDA_DIR / "fft_cuda.cu"),
                "-o",
                str(CUDA_DIR / "fft_cuda"),
            ]
        )


def execution_time(args, env=None):
    output = command(args, env)
    match = TIME_PATTERN.search(output)
    if not match:
        raise RuntimeError(f"Could not read execution time from:\n{output}")
    return float(match.group(1))


def save_results(folder, title, configuration_name, results):
    baseline = results[0][1]
    lines = [
        title,
        "=" * len(title),
        "Dataset size: 1,048,576 samples",
        "Each configuration was run once.",
        "",
        f"{configuration_name:<16} {'Time (seconds)':>16} {'Speedup':>12}",
        "-" * 46,
    ]
    for configuration, time in results:
        lines.append(
            f"{str(configuration):<16} {time:>16.6f} {baseline / time:>12.4f}"
        )
    (folder / "results.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def line_graph(results, title, xlabel, ylabel, filename, speedup=False):
    x = [item[0] for item in results]
    baseline = results[0][1]
    y = [baseline / item[1] for item in results] if speedup else [item[1] for item in results]

    plt.figure(figsize=(8, 5))
    plt.plot(x, y, marker="o", linewidth=2)
    plt.title(title)
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.xticks(x)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(filename, dpi=200)
    plt.close()


def save_graphs(folder, name, configuration_label, results):
    line_graph(
        results,
        f"{name}: {configuration_label} vs Execution Time",
        configuration_label,
        "Execution time (seconds)",
        folder / "graphs" / "execution_time.png",
    )
    line_graph(
        results,
        f"{name}: {configuration_label} vs Speedup",
        configuration_label,
        "Speedup",
        folder / "graphs" / "speedup.png",
        speedup=True,
    )


def run_openmp():
    results = []
    for threads in CONFIGURATIONS:
        env = os.environ.copy()
        env["OMP_NUM_THREADS"] = str(threads)
        env["OMP_DYNAMIC"] = "FALSE"
        results.append((threads, execution_time([str(OPENMP_DIR / "fft_omp")], env)))
    folder = EVAL_DIR / "OpenMP"
    save_results(folder, "OpenMP Performance Results", "Threads", results)
    save_graphs(folder, "OpenMP", "Number of threads", results)
    return results


def run_mpi():
    results = [
        (
            processes,
            execution_time(
                ["mpirun", "--oversubscribe", "-np", str(processes), str(MPI_DIR / "fft_mpi")]
            ),
        )
        for processes in CONFIGURATIONS
    ]
    folder = EVAL_DIR / "MPI"
    save_results(folder, "MPI Performance Results", "Processes", results)
    save_graphs(folder, "MPI", "Number of processes", results)
    return results


def run_cuda():
    folder = EVAL_DIR / "CUDA"
    if not (CUDA_DIR / "fft_cuda").exists():
        results_file = folder / "results.txt"
        if results_file.exists():
            results = []
            for line in results_file.read_text(encoding="utf-8").splitlines():
                values = line.split()
                if len(values) == 3 and values[0].isdigit():
                    results.append((int(values[0]), float(values[1])))
            if results:
                return results
        return []

    results = [
        (block_size, execution_time([str(CUDA_DIR / "fft_cuda"), str(block_size)]))
        for block_size in CUDA_BLOCK_SIZES
    ]
    save_results(folder, "CUDA Performance Results", "Block size", results)
    save_graphs(folder, "CUDA", "Threads per block", results)
    return results


def comparative_graph(labels, values, title, ylabel, filename):
    plt.figure(figsize=(9, 5))
    bars = plt.bar(labels, values)
    plt.title(title)
    plt.ylabel(ylabel)
    plt.grid(axis="y", alpha=0.3)
    for bar, value in zip(bars, values):
        plt.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height(),
            f"{value:.3f}",
            ha="center",
            va="bottom",
        )
    plt.tight_layout()
    plt.savefig(filename, dpi=200)
    plt.close()


def save_comparison(serial_time, openmp, mpi, cuda):
    implementations = [
        ("Serial", "1 thread", serial_time),
        ("OpenMP", f"{min(openmp, key=lambda item: item[1])[0]} threads", min(openmp, key=lambda item: item[1])[1]),
        ("MPI", f"{min(mpi, key=lambda item: item[1])[0]} processes", min(mpi, key=lambda item: item[1])[1]),
    ]
    if cuda:
        best_cuda = min(cuda, key=lambda item: item[1])
        implementations.append(("CUDA", f"block size {best_cuda[0]}", best_cuda[1]))

    folder = EVAL_DIR / "Comparative"
    labels = [f"{name}\n{configuration}" for name, configuration, _ in implementations]
    times = [time for _, _, time in implementations]
    speedups = [serial_time / time for time in times]

    comparative_graph(
        labels,
        times,
        "FFT Execution Time Comparison",
        "Execution time (seconds)",
        folder / "graphs" / "execution_time_comparison.png",
    )
    comparative_graph(
        labels,
        speedups,
        "FFT Speedup Comparison",
        "Speedup compared with serial",
        folder / "graphs" / "speedup_comparison.png",
    )

    fastest = min(implementations, key=lambda item: item[2])
    lines = [
        "Comparison Results",
        "==================",
        "Dataset size: 1,048,576 samples",
        "",
        f"{'Implementation':<16} {'Best configuration':<20} {'Time (seconds)':>16} {'Speedup':>10}",
        "-" * 68,
    ]
    for name, configuration, time in implementations:
        lines.append(
            f"{name:<16} {configuration:<20} {time:>16.6f} {serial_time / time:>10.4f}"
        )
    lines.extend(
        [
            "",
            "Simple Analysis",
            "---------------",
            f"{fastest[0]} was the fastest implementation in this test.",
            "OpenMP is simple and works well on one computer, but adding more threads gives smaller improvements after a point.",
            "MPI can use many processes and can run across multiple computers, but communication between processes adds extra work.",
        ]
    )
    if cuda:
        lines.append(
            "CUDA can process many FFT operations at the same time, but it requires an NVIDIA GPU."
        )
    else:
        lines.append(
            "CUDA was not compared because this computer does not have an NVIDIA CUDA GPU."
        )
    (folder / "results.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    build()
    serial_time = execution_time([str(SERIAL_DIR / "fft_serial")])
    openmp = run_openmp()
    mpi = run_mpi()
    cuda = run_cuda()
    save_comparison(serial_time, openmp, mpi, cuda)
    print("Evaluation complete.")


if __name__ == "__main__":
    main()
