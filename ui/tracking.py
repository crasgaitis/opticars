from utils import build_dataset, get_tracker, detect_movement_example_with_scaling, calculatePower
import pandas as pd
import time

tracker = get_tracker()

while True:
    data, dict_list = build_dataset(tracker, 'test', time_step_sec=0.15, tot_time_min=0.0025)
    data2 = pd.DataFrame(data.iloc[0]).transpose()
    print(data)
    mag, dir = detect_movement_example_with_scaling(data2)
    print(f"Mag: {mag}, Dir: {dir}")

    leftMag, rightMag = calculatePower(data2)
    print(f"Left: {leftMag}, Right: {rightMag}")
    # time.sleep(0.1)