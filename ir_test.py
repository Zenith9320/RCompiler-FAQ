#!/usr/bin/env python3
import os
import subprocess
import shutil
import sys

def run_command(cmd, cwd=None, timeout=30):
    try:
        result = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True, timeout=timeout)
        return result.returncode == 0, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return False, "", "Timeout"
    except Exception as e:
        return False, "", str(e)

def main():
    base_dir = "."
    src_dir = os.path.join("RCompiler-Testcases", "IR-1", "src")
    testcases_dir = "testcases"

    if not os.path.exists(src_dir):
        print("Source directory not found")
        return

    passed = 0
    total = 0

    for i in range(1, 51):
        folder = f"comprehensive{i}"
        folder_path = os.path.join(src_dir, folder)
        if not os.path.exists(folder_path):
            continue

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
            print(f"\033[91m[FAIL] {folder}: Missing files\033[0m")
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
            print(f"\033[91m[FAIL] {folder}: Failed to copy .rx: {e}\033[0m")
            continue

        # Run ./ir_test
        success, stdout, stderr = run_command("./ir_test", cwd=base_dir, timeout=60)
        if not success:
            print(f"\033[91m[FAIL] {folder}: ir_test failed: {stderr}\033[0m")
            continue

        # Copy testcases/1.out to testcases/1.ll
        try:
            shutil.copy(os.path.join(testcases_dir, "1.out"), os.path.join(testcases_dir, "1.ll"))
        except Exception as e:
            print(f"\033[91m[FAIL] {folder}: Failed to copy .out to .ll: {e}\033[0m")
            continue

        # Run clang
        success, stdout, stderr = run_command(f"clang {os.path.join(testcases_dir, '1.ll')} -o {os.path.join(testcases_dir, '1')}", timeout=60)
        if not success:
            print(f"\033[91m[FAIL] {folder}: clang failed: {stderr}\033[0m")
            continue

        # Run the executable with .in as input
        try:
            with open(in_file, 'r') as f:
                input_data = f.read()
            success, stdout, stderr = run_command(f"./{os.path.join(testcases_dir, '1')}", cwd=base_dir, timeout=10)
            if not success:
                print(f"\033[91m[FAIL] {folder}: Executable failed: {stderr}\033[0m")
                continue
            # Read expected output
            with open(out_file, 'r') as f:
                expected = f.read().strip()
            actual = stdout.strip()
            if actual == expected:
                print(f"\033[92m[PASS] {folder}\033[0m")
                passed += 1
            else:
                print(f"\033[91m[FAIL] {folder}: Output mismatch\033[0m")
        except Exception as e:
            print(f"\033[91m[FAIL] {folder}: Execution error: {e}\033[0m")

    print(f"\nPassed: {passed}/{total}")

if __name__ == "__main__":
    main()