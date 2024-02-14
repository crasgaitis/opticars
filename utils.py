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
    
# gaze id takes in an x and y coordinate and returns the id that should be highlighted
def gaze_id(dataframe):
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

    x = (translate2ScreenX(left_x_values[0]) + translate2ScreenX(right_x_values[0])) / 2
    y = (translate2ScreenY(left_y_values[0]) + translate2ScreenY(right_y_values[0])) / 2

    element = "o"
    if (x < -0.33 and y > 0.33):
        element += "1"
    elif (x > -0.33 and x < 0.33 and y > 0.33):
        element += "2"
    elif (x > 0.33 and y > 0.33):
        element += "3"
    elif (x < -0.33 and y > -0.33 and y < 0.33):
        element += "4"
    elif (x > -0.33 and x < 0.33 and y > -0.33 and y < 0.33):
        element += "5"
    elif (x > 0.33 and y > -0.33 and y < 0.33):
        element += "6"
    elif (x < -0.33 and y < -0.33):
        element += "7"
    elif (x > -0.33 and x < 0.33 and y < -0.33):
        element += "8"
    elif (x > 0.33 and y < -0.33):
        element += "9"

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
def calculatePower(left_x, left_y, right_x, right_y):
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
    if output < -1:
        return -1
    elif output > 1:
        return 1
    return output

# translate2ScreenY takes in a value and returns the translated y coordinate
def translate2ScreenY(ycoord):
    output = 1 - 2*ycoord
    if output < -1:
        return -1
    elif output > 1:
        return 1
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
