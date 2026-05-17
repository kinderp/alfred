import os
import time
import subprocess

ROOT="/tmp/fsmon_bench"
BIN="../../fsmon"

os.system("rm -rf " + ROOT)
os.makedirs(ROOT)

proc = subprocess.Popen([BIN, ROOT])

time.sleep(1)

start = time.time()

for i in range(20000):
    open(f"{ROOT}/x{i}.txt","w").close()

end = time.time()

proc.send_signal(2)
proc.wait()

print("events/sec:", 20000/(end-start))
