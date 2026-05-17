import os
import subprocess
import time
import shutil

ROOT="/tmp/fsmon_stress_move"
BIN="../../fsmon"

shutil.rmtree(ROOT, ignore_errors=True)
os.makedirs(ROOT)

src = os.path.join(ROOT, "src")
dst = os.path.join(ROOT, "dst")

os.makedirs(src)
os.makedirs(dst)

proc = subprocess.Popen([BIN, ROOT])

time.sleep(1)

for i in range(5000):
    f = f"{src}/f{i}.txt"
    open(f, "w").write("x")
    os.rename(f, f"{dst}/f{i}.txt")

time.sleep(2)

proc.send_signal(2)
proc.wait()

print("stress move done")
