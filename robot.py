import sys
import time
import random

fps = 20
start_features = 2000
start_std = 20

start = time.time() * 1000.0
for t in range(20 * fps):
    start_features = 4 + start_features
    if t % fps == 0:
        print(t / fps)
    should_fail = False
    iter = int(random.gauss(start_features, start_std))
    if t == 15 * fps:
        iter = 20000
        should_fail = True

    try:
        with open("/proc/my_data_in", "w") as f:
            f.write(str(iter))
    except:
        if should_fail:
            print("Detected attack")
        else:
            print("Detected false attack")
        break
    if should_fail:
        print("Did not detect attack")
        break
    now = time.time() * 1000.0

    time.sleep((1 / 1000) * ((1000 / fps) - (now - start)))
    start = time.time() * 1000.0
