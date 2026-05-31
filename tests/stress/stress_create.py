import os
import subprocess
import time

ROOT="/tmp/alfred_stress"
BIN="../../alfred"

os.system("rm -rf " + ROOT)
os.makedirs(ROOT)

proc = subprocess.Popen([BIN, ROOT])

time.sleep(1)

for i in range(10000):
    open(f"{ROOT}/file_{i}.txt", "w").write("x")

time.sleep(2)

proc.send_signal(2)
proc.wait()

print("stress create done")
