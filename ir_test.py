#!/usr/bin/env python3
import os
import subprocess
import shutil
import sys
import multiprocessing
import threading
import time

def run_command(cmd, cwd=None, timeout=6, input=None):
    try:
        result = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True, timeout=timeout, input=input)
        return result.returncode == 0, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return False, "", "Timeout"
    except Exception as e:
        return False, "", str(e)

def run_single_test(i):
    folder = f"comprehensive{i}"
    folder_path = os.path.join("RCompiler-Testcases", "IR-1", "src", folder)
    if not os.path.exists(folder_path):
        return None

    rx_file = None
    in_file = None
    out_file = None
    for f in os.listdir(folder_path):
        if f.endswith(".rx"):
            rx_file = os.path.join(folder_path, f)
        elif f.endswith(".in"):
            in_file = os.path.join(folder_path, f)
        elif f.endswith(".out"):
            out_file = os.path.join(folder_path, f)

    if not rx_file or not in_file or not out_file:
        return f"\033[91m[FAIL] {folder}: Missing files\033[0m"

    # Copy .rx content to testcases/1.data
    try:
        with open(rx_file, 'r') as f:
            content = f.read()
        with open(os.path.join("testcases", "1.data"), 'w') as f:
            f.write(content)
    except Exception as e:
        return f"\033[91m[FAIL] {folder}: Failed to copy .rx: {e}\033[0m"

    # Run ./ir_test
    success, stdout, stderr = run_command("./ir_test", timeout=5)
    if not success:
        return f"\033[91m[FAIL] {folder}: ir_test failed: {stderr}\033[0m"

    # Copy testcases/1.out to testcases/1.ll
    try:
        shutil.copy(os.path.join("testcases", "1.out"), os.path.join("testcases", "1.ll"))
    except Exception as e:
        return f"\033[91m[FAIL] {folder}: Failed to copy .out to .ll: {e}\033[0m"

    # Run clang with -O0 for faster compilation
    success, stdout, stderr = run_command(f"clang -O0 testcases/1.ll -o testcases/1", timeout=7200)
    if not success:
        return f"\033[91m[FAIL] {folder}: clang failed: {stderr}\033[0m"

    # Run the executable with .in as input
    try:
        with open(in_file, 'r') as f:
            input_data = f.read()
        success, stdout, stderr = run_command("ulimit -s unlimited && ./testcases/1", input=input_data, timeout=300)
        if not success:
            return f"\033[91m[FAIL] {folder}: Executable failed: {stderr}\033[0m"
        # Read expected output
        with open(out_file, 'r') as f:
            expected = f.read().strip()
        actual = stdout.strip()
        if actual == expected:
            return f"\033[92m[PASS] {folder}\033[0m"
        else:
            return f"\033[91m[FAIL] {folder}: Output mismatch\033[0m"
    except Exception as e:
        return f"\033[91m[FAIL] {folder}: Execution error: {e}\033[0m"

def main():
    start_time = time.time()
    base_dir = "."
    src_dir = os.path.join("RCompiler-Testcases", "IR-1", "src")
    testcases_dir = "testcases"

    if not os.path.exists(src_dir):
        print("Source directory not found")
        return

    passed = 0
    total = 0
    total_test_time = 0.0

    for i in range(1, 51):
        folder = f"comprehensive{i}"
        folder_path = os.path.join(src_dir, folder)
        if not os.path.exists(folder_path):
            continue
    
        start_time = time.time()
        rx_file = None
        in_file = None
        out_file = None
        for f in os.listdir(folder_path):
            if f.endswith(".rx"):
                rx_file = os.path.join(folder_path, f)
            elif f.endswith(".in"):
                in_file = os.path.join(folder_path, f)
            elif f.endswith(".out"):
                out_file = os.path.join(folder_path, f)

        if not rx_file or not in_file or not out_file:
            elapsed = time.time() - start_time
            total_test_time += elapsed
            print(f"\033[91m[FAIL] {folder}: Missing files ({elapsed:.2f}s)\033[0m")
            total += 1
            continue

        total += 1

        # Copy .rx content to testcases/1.data
        try:
            with open(rx_file, 'r') as f:
                content = f.read()
            with open(os.path.join(testcases_dir, "1.data"), 'w') as f:
                f.write(content)
        except Exception as e:
            elapsed = time.time() - start_time
            total_test_time += elapsed
            print(f"\033[91m[FAIL] {folder}: Failed to copy .rx: {e} ({elapsed:.2f}s)\033[0m")
            continue

        # Run ./ir_test
        success, stdout, stderr = run_command("./ir_test", cwd=base_dir, timeout=5)
        if not success:
            elapsed = time.time() - start_time
            total_test_time += elapsed
            print(f"\033[91m[FAIL] {folder}: ir_test failed: {stderr} ({elapsed:.2f}s)\033[0m")
            continue

        # Copy testcases/1.out to testcases/1.ll
        try:
            shutil.copy(os.path.join(testcases_dir, "1.out"), os.path.join(testcases_dir, "1.ll"))
        except Exception as e:
            elapsed = time.time() - start_time
            total_test_time += elapsed
            print(f"\033[91m[FAIL] {folder}: Failed to copy .out to .ll: {e} ({elapsed:.2f}s)\033[0m")
            continue

        # Run clang with -O0 for faster compilation
        success, stdout, stderr = run_command(f"clang -O0 {os.path.join(testcases_dir, '1.ll')} -o {os.path.join(testcases_dir, '1')}", timeout=3600)
        if not success:
            elapsed = time.time() - start_time
            total_test_time += elapsed
            print(f"\033[91m[FAIL] {folder}: clang failed: {stderr} ({elapsed:.2f}s)\033[0m")
            continue

        # Run the executable with .in as input
        try:
            with open(in_file, 'r') as f:
                input_data = f.read()
            success, stdout, stderr = run_command(f"ulimit -s unlimited && ./{os.path.join(testcases_dir, '1')}", cwd=base_dir, timeout=3600, input=input_data)
            if not success:
                elapsed = time.time() - start_time
                total_test_time += elapsed
                print(f"\033[91m[FAIL] {folder}: Executable failed: {stderr} ({elapsed:.2f}s)\033[0m")
                continue
            # Read expected output
            with open(out_file, 'r') as f:
                expected = f.read().strip()
            actual = stdout.strip()
            if actual == expected:
                elapsed = time.time() - start_time
                total_test_time += elapsed
                print(f"\033[92m[PASS] {folder} ({elapsed:.2f}s)\033[0m")
                passed += 1
            else:
                elapsed = time.time() - start_time
                total_test_time += elapsed
                print(f"\033[91m[FAIL] {folder}: Output mismatch ({elapsed:.2f}s)\033[0m")
        except Exception as e:
            elapsed = time.time() - start_time
            total_test_time += elapsed
            print(f"\033[91m[FAIL] {folder}: Execution error: {e} ({elapsed:.2f}s)\033[0m")

    print(f"\nPassed: {passed}/{total}")
    print(f"Total execution time: {total_test_time:.2f} seconds")
if __name__ == "__main__":
    main()