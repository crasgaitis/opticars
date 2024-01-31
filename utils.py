import time 
import tobii_research as tr
import pandas as pd
import math
import ast

def combine_dicts_with_labels(dict_list):
    combined_dict = {}
    for i, dictionary in enumerate(dict_list, start=1):
        label = f"timestep_{i}"
        combined_dict[label] = dictionary

    return combined_dict

def gaze_data_callback(gaze_data):
  global global_gaze_data
  global_gaze_data = gaze_data
  
def gaze_data(eyetracker, wait_time=5):
    #records for wait_time number of seconds
  global global_gaze_data

  # print("Getting data...")
  eyetracker.subscribe_to(tr.EYETRACKER_GAZE_DATA, gaze_data_callback, as_dictionary=True)

  time.sleep(wait_time)

  eyetracker.unsubscribe_from(tr.EYETRACKER_GAZE_DATA, gaze_data_callback)

  return global_gaze_data

def build_dataset(tracker, label, add_on = False, df_orig = pd.DataFrame(), 
                  time_step_sec = 0.5, tot_time_min = 0.1):
    
    global global_gaze_data
    
    intervals = math.ceil((tot_time_min * 60) / time_step_sec)
    dict_list = []
    
    for _ in range(intervals):
        data = gaze_data(tracker, time_step_sec)
        # print(data)
        dict_list.append(data)
    
    tot_dict = combine_dicts_with_labels(dict_list)
    df = pd.DataFrame(tot_dict).T
    df['type'] = label
        
    if add_on:
        df_new = pd.concat([df_orig, df])
        df_new = df_new.reset_index(drop=True)
        return df_new
    
    else:
        return df, dict_list

def build_dataset_from_csv(file_path, label):
    '''
    Builds a dataframe from a csv file. Revives tuples from string format.
    file_path: path to csv file
    label: label for the data
    add_on: if True, add to existing dataframe
    df_orig: original dataframe to add to

    returns: dataframe with data from csv file
    '''

    # all the columns that are tuples
    # TODO: compute programmatically
    tuples = ['left_gaze_point_on_display_area',
    'left_gaze_point_in_user_coordinate_system',
    'left_gaze_origin_in_user_coordinate_system',
    'left_gaze_origin_in_trackbox_coordinate_system',
    'right_gaze_point_on_display_area',
    'right_gaze_point_in_user_coordinate_system',
    'right_gaze_origin_in_user_coordinate_system',
    'right_gaze_origin_in_trackbox_coordinate_system']

    df = pd.read_csv(file_path, converters={key: ast.literal_eval for key in tuples})
    df['type'] = label
    return df

# calculatePower takes in 4 args (left and right eye coords) and returns left and right magnitude
def calculatePower(left_x, left_y, right_x, right_y):
    leftMagnitude = (left_y + right_y)/2 + (left_x + right_x)/2
    rightMagnitude = (left_y + right_y)/2 - (left_x + right_x)/2

    if abs(leftMagnitude) > 1.0:
        leftMagnitude /= abs(leftMagnitude)

    if abs(rightMagnitude) > 1.0:
        rightMagnitude /= abs(rightMagnitude)
        
    return leftMagnitude, rightMagnitude