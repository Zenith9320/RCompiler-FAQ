import os
import re
import subprocess
import shutil

root_dir = "RCompiler-Testcases/semantic-2/src"
exe_name = "semantic_test"
timeout_limit = 20

GREEN = "\033[92m"
RED = "\033[91m"
RESET = "\033[0m"

# 确保 testcases/outputs 文件夹存在
output_root = "testcases/outputs"
os.makedirs(output_root, exist_ok=True)

# 编译程序
if not os.path.exists(exe_name):
    print("编译中 ...")
    # 使用和手动编译相同的命令
    res = subprocess.run(
        ["g++", "src/lexer.cpp", "semantic_test.cpp", "-lboost_regex", "-o", exe_name],
        cwd=".",  # 当前工作目录为脚本所在目录
    )
    if res.returncode != 0:
        print("编译失败！")
        exit(1)
    print("编译成功")

# 收集所有 .rx 文件
rx_files = []
for dirpath, _, filenames in os.walk(root_dir):
    for f in filenames:
        if f.endswith(".rx"):
            rx_files.append(os.path.join(dirpath, f))

if not rx_files:
    print("未找到任何 .rx 文件！")
    exit(0)

passed, total = 0, 0

for rx_path in sorted(rx_files):
    total += 1
    relative_path = os.path.relpath(rx_path, root_dir)

    # 读取 Verdict
    with open(rx_path, "r", encoding="utf-8") as f:
        content = f.read()
    verdict_match = re.search(r"Verdict:\s*(Pass|Fail)", content)
    if not verdict_match:
        print(f"{relative_path} 缺少 Verdict 字段，跳过。")
        continue
    verdict = verdict_match.group(1)
    expected = "0" if verdict == "Pass" else "-1"

    # 生成输入文件（程序仍然读 testcases/1.data）
    os.makedirs("testcases", exist_ok=True)
    shutil.copyfile(rx_path, "testcases/1.data")

    # 生成输出文件名，根据 rx 文件路径
    output_name = relative_path.replace(os.sep, "_").replace(".rx", ".out")
    output_path = os.path.join(output_root, output_name)

    # 运行程序
    try:
        result = subprocess.run(
            [f"./{exe_name}"],
            timeout=timeout_limit,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd="."  # 保证程序在正确目录运行
        )
    except subprocess.TimeoutExpired:
        print(f"{RED}[Fail]{RESET} {relative_path} (Timeout)")
        continue

    # 检查程序返回码
    if result.returncode < 0:
        print(f"{RED}[Fail]{RESET} {relative_path} (Segmentation fault)")
        continue
    elif result.returncode != 0:
        print(f"{RED}[Fail]{RESET} {relative_path} (Runtime error)")
        continue

    # 检查输出文件
    original_out = "testcases/1.out"
    if not os.path.exists(original_out):
        print(f"{RED}[Fail]{RESET} {relative_path} (No output file)")
        continue

    # 将输出文件复制到独立文件
    shutil.copyfile(original_out, output_path)

    # 读取最后一行作为实际输出
    with open(output_path, "r", encoding="utf-8") as f:
        lines = [line.strip() for line in f if line.strip()]
    actual = lines[-1] if lines else "(empty)"

    # 判断 Pass/Fail
    if actual == expected:
        print(f"{GREEN}[Pass]{RESET} {relative_path}")
        passed += 1
    else:
        print(f"{RED}[Fail]{RESET} {relative_path} (expected {expected}, got {actual})")

print(f"\n共 {total} 个样例，正确 {passed}，错误 {total - passed}")
