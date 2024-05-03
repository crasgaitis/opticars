import time 
import tobii_research as tr
import pandas as pd
import math
import ast
import threading
import numpy as np

global_gaze_data = None
lock = threading.Lock()

def combine_dicts_with_labels(dict_list):
    combined_dict = {}
    for i, dictionary in enumerate(dict_list, start=1):
        label = f"timestep_{i}"
        combined_dict[label] = dictionary

    return combined_dict

def gaze_data_callback(gaze_data):
  global global_gaze_data
  with lock:
    global_gaze_data = gaze_data
  
def gaze_data(eyetracker, wait_time=5):
  global global_gaze_data
  
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
        
    # print(data)
    
    tot_dict = combine_dicts_with_labels(dict_list)
    index = range(intervals)

    df = pd.DataFrame(tot_dict, index=index).T
    df['type'] = label
        
    if add_on:
        df_new = pd.concat([df_orig, df])
        df_new = df_new.reset_index(drop=True)
        return df_new
    
    else:
        return df, dict_list
    
# gaze id takes in an x and y coordinate and returns the id that should be highlighted
def preprocess_gaze(dataframe):
    #if right eye is invalid, use left eye data
    if dataframe['right_gaze_point_validity'] == 0:
        left_gp = dataframe['left_gaze_point_on_display_area']
        right_gp = dataframe['left_gaze_point_on_display_area']
    #if left eye is invalid, use right eye data
    elif dataframe['left_gaze_point_validity'] == 0:
        left_gp = dataframe['right_gaze_point_on_display_area']
        right_gp = dataframe['right_gaze_point_on_display_area']
    # if both valid
    elif (dataframe['left_gaze_point_validity'] == 1 and dataframe['right_gaze_point_validity'] == 1):
        left_gp = dataframe['left_gaze_point_on_display_area']
        right_gp = dataframe['right_gaze_point_on_display_area']
    else:
        print('ouch!')
        return "o1" 
        # returns now so it wont be overwritten later

    # extract x and y coordinates from the specified column
    left_x_values = [point[0] for point in [left_gp]]
    left_y_values = [point[1] for point in [left_gp]]
    right_x_values = [point[0] for point in [right_gp]]
    right_y_values = [point[1] for point in [right_gp]]

    # Translate the x and y coordinates
    left_x_values = [translate2ScreenX(x) for x in left_x_values]
    left_y_values = [translate2ScreenY(y) for y in left_y_values]
    right_x_values = [translate2ScreenX(x) for x in right_x_values]
    right_y_values = [translate2ScreenY(y) for y in right_y_values]
    
    # print(np.round(left_x_values[0], 1))
    # print(np.round(left_y_values[0], 1))

    return left_x_values, left_y_values, right_x_values, right_y_values


def gaze_id(gazexy):
    
    left_x_values, left_y_values, right_x_values, right_y_values = gazexy
    
    gx = (left_x_values[0] + right_x_values[0])/2
    gy = (left_y_values[0] + right_y_values[0])/2 
    
    if gx > 2:
        gx = 2
    if gy > 2:
        gy = 2
    
    element = "o"
    
    if (gx < -0.4 and gy > .35):
        element += "1"
    elif (gx < .2 and gy > .35):
        element += "2"
    elif (gx >= .2 and gy > .35):
        element += "3"
    elif (gx < -0.4 and gy > -.25):
        element += "4"
    elif (gx < .2 and gy > -.25):
        element += "5"
    elif (gx >= .2 and gy > -.25):
        element += "6"
    elif (gx < -0.4 and gy <= -.25):
        element += "7"
    elif (gx < .2 and gy <= -.25):
        element += "8"
    elif (gx >= .2 and gy <= -.25):
        element += "9"
    # print(element)

    return element
     
def safe_tuple_eval(s, default_value=None):
    """
    Safely evaluates a string to convert it to a tuple. Returns a default value if the string
    is NaN or cannot be converted.

    Args:
    s: str, the string to be evaluated.
    default_value: The default value to return if conversion fails or s is NaN. Defaults to None.

    Returns:
    The evaluated tuple or the default value.
    """
    if pd.isna(s):
        # Return the default value if the value is NaN
        return default_value
    try:
        # Attempt to convert string to tuple
        return ast.literal_eval(s)
    except (ValueError, SyntaxError):
        # Return default value if conversion fails
        return default_value

def safe_tuple_eval_for_dict(s, default_value = None):
    try:
        return ast.literal_eval(s)
    except (ValueError, SyntaxError):
        return default_value

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

    #converters = {key: safe_tuple_eval for key in tuples}
    converters = {key: lambda s: safe_tuple_eval(s, default_value=None) for key in tuples}
    # converters = {key: lambda s: safe_tuple_eval(s, default_value=(0, 0)) for key in tuples}
    
    df = pd.read_csv(file_path, converters=converters)
    #df = pd.read_csv(file_path, converters={key: ast.literal_eval for key in tuples})
    df['type'] = label
    df['left_pupil_diameter'].fillna("None", inplace=True)
    df['right_pupil_diameter'].fillna("None", inplace=True)
    #df = df.applymap(lambda x: None if pd.isna(x) else x)
    return df

# calculatePower takes in 4 args (left and right eye coords) and returns left and right magnitude
# def calculatePower(left_x, left_y, right_x, right_y):
#     leftMagnitude = (left_y + right_y)/2 + (left_x + right_x)/2
#     rightMagnitude = (left_y + right_y)/2 - (left_x + right_x)/2

#     if abs(leftMagnitude) > 1.0:
#         leftMagnitude /= abs(leftMagnitude)

#     if abs(rightMagnitude) > 1.0:
#         rightMagnitude /= abs(rightMagnitude)
        
#     return leftMagnitude, rightMagnitude

def rescale_item(item, min_value=-1.2, max_value = 1.2):
    """Rescales each value within the item to 1 to -1, based on the specified min and max values. """
    rescaled_item = []
    for value in item:
        if value < min_value:
            value = min_value
        elif value > max_value:
            value = max_value
        rescaled_value = 2 * (value - min_value) / (max_value - min_value) - 1
        rescaled_item.append(rescaled_value)
    return rescaled_item

def calculatePower_new(gazexy):
    
    left_x, left_y, right_x, right_y = gazexy
    
    # left_x, right_x = rescale_item((left_x[0], right_x[0]), -1.2, 1.2) 
    # left_y, right_y = rescale_item((left_y[0] * -1, right_y[0] * -1), -1.2, 1.2) 

    leftMagnitude = (left_y + right_y)/2 + (left_x + right_x)/2
    rightMagnitude = (left_y + right_y)/2 - (left_x + right_x)/2

    if abs(leftMagnitude) > 2.0:
        leftMagnitude /= abs(leftMagnitude)

    if abs(rightMagnitude) > 2.0:
        rightMagnitude /= abs(rightMagnitude)
        
def calculatePower_new2(gazexy):
    left_x, left_y, right_x, right_y = gazexy
    
    gx = (left_x[0] + right_x[0])/2
    gy = (left_y[0] + right_y[0])/2
        
    if (gx < -0.4 and gy > .35):
        left = 1.2
        right = 2
    elif (gx < .2 and gy > .35):
        left = 2
        right = 2
    elif (gx >= .2 and gy > .35):
        left = 2
        right = 1.2
    elif (gx < -0.4 and gy > -.25):
        left = 1
        right = 2
    elif (gx < .2 and gy > -.25):
        left = 1
        right = 1
    elif (gx >= .2 and gy > -.25):
        left = 2
        right = 1
    elif (gx < -0.4 and gy <= -.25):
        left = 1.2
        right = 0.2
    elif (gx < .2 and gy <= -.25):
        left = 0
        right = 0
    elif (gx >= .2 and gy <= -.25):
        left = 0.2
        right = 1.2
    
    return left, right

        
    
    # leftMagnitude, rightMagnitude = rescale_item((leftMagnitude, rightMagnitude), -1, 1)  
        
    return np.round(leftMagnitude, 1), np.round(rightMagnitude, 1)

def calculatePowerold(dataframe):
    left_x, left_y, right_x, right_y = parse_gaze_data(dataframe)

    leftMagnitude = (left_y + right_y)/2 + (left_x + right_x)/2
    rightMagnitude = (left_y + right_y)/2 - (left_x + right_x)/2

    if abs(leftMagnitude) > 1.0:
        leftMagnitude /= abs(leftMagnitude)

    if abs(rightMagnitude) > 1.0:
        rightMagnitude /= abs(rightMagnitude)
        
    return leftMagnitude, rightMagnitude

# translate2ScreenX takes in a value and returns the translated x coordinate
def translate2ScreenX(xcoord):
    output = 2*xcoord - 1
    # if output < -1:
    #     return -1
    # elif output > 1:
    #     return 1
    return output

# translate2ScreenY takes in a value and returns the translated y coordinate
def translate2ScreenY(ycoord):
    output = 1 - 2*ycoord
    # if output < -1:
    #     return -1
    # elif output > 1:
    #     return 1
    return output

# gaze_detection takes in a dataframe and column name and returns x and y coordinates
# - column_name must be left eye or right eye data values
def gaze_detection(dataframe, column_name):
    # extract x and y coordinates from the specified column
    x_values = [point[0] for point in dataframe[column_name]]
    y_values = [point[1] for point in dataframe[column_name]]
    x_value = translate2ScreenX(x_values[0])
    y_value = translate2ScreenY(y_values[0])

    # assume that there's only one value in x- and y-values
    
    # return an id from 01 to 09
    return x_values, y_values

def get_tracker():
  all_eyetrackers = tr.find_all_eyetrackers()

  for tracker in all_eyetrackers:
    # print("Model: " + tracker.model)
    # print("Serial number: " + tracker.serial_number) 
    # print(f"Can stream eye images: {tr.CAPABILITY_HAS_EYE_IMAGES in tracker.device_capabilities}")
    # print(f"Can stream gaze data: {tr.CAPABILITY_HAS_GAZE_DATA in tracker.device_capabilities}")
    return tracker

def parse_gaze_data(dataframe):
     # extract x and y coordinates from the specified column
    left_x_values = [point[0] for point in dataframe['left_gaze_point_on_display_area']]
    left_y_values = [point[1] for point in dataframe['left_gaze_point_on_display_area']]
    right_x_values = [point[0] for point in dataframe['right_gaze_point_on_display_area']]
    right_y_values = [point[1] for point in dataframe['right_gaze_point_on_display_area']]

    left_x_values = [translate2ScreenX(x) for x in left_x_values]
    left_y_values = [translate2ScreenY(y) for y in left_y_values]
    right_x_values = [translate2ScreenX(x) for x in right_x_values]
    right_y_values = [translate2ScreenY(y) for y in right_y_values]

    # take the average of all left_x_values and left_y_values

    left_x = mean(left_x_values)
    left_y = mean(left_y_values)
    right_x = mean(right_x_values)
    right_y = mean(right_y_values)

    return left_x, left_y, right_x, right_y

# mean takes in a list and returns the mean of the list
def mean(list):
    return sum(list) / len(list)