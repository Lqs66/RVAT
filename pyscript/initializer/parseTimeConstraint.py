import struct
import pandas as pd
from collections import defaultdict
from ..pyTools.utils import *

__all__ = ["getTimeBounds"]

def parseLoggedTime(path :str)-> dict:
    bb_time_data = defaultdict(list)

    with open(path, 'rb') as f:
        while True:
            data = f.read(16)
            if not data:
                break
            
            if len(data) == 16:
                bbNum, diff = struct.unpack('<QQ', data)
                bb_time_data[bbNum].append(diff)

    result = {}
    for bbNum, diff_list in bb_time_data.items():
        result[bbNum] = pd.Series(
            data=diff_list,
            dtype='int64',
            name=f'bb_{bbNum}_time'
        )
    
    return result

def gevBasedTimeAnalysis(data: pd.Series, block_size: int = 100, alpha: float = 0.98):
    from pygev.gev import EVA
    import warnings
    
    # Get actual min and max values
    actual_min = int(data.min())
    actual_max = int(data.max())
    
    # Check if all data values are the same
    if actual_min == actual_max:
        raise ValueError(f"All data values are identical ({actual_min}), cannot perform GEV analysis")
    
    # Check data variance
    if data.std() < 1e-6:
        raise ValueError(f"Data variance too small ({data.std()}), cannot perform GEV analysis")
    
    # Suppress scipy warnings
    with warnings.catch_warnings():
        warnings.filterwarnings('ignore', category=RuntimeWarning)
        
        eva = EVA(data=data)
        
        # Calculate upper bound
        eva.get_extremes(extremes_type='high', block_size=block_size)
        if len(eva.extremes) < 3:
            raise ValueError(f"Too few maximum values extracted ({len(eva.extremes)}), consider reducing block_size")
        eva.fit_model()
        upper_bound_transformed = eva.model.isf(1 - alpha)
        upper_bound = eva.extremes_transformer.transform(upper_bound_transformed)
        
        # Calculate lower bound
        eva.get_extremes(extremes_type='low', block_size=block_size)
        if len(eva.extremes) < 3:
            raise ValueError(f"Too few minimum values extracted ({len(eva.extremes)}), consider reducing block_size")
        eva.fit_model()
        lower_bound_transformed = eva.model.ppf(1 - alpha)
        lower_bound = eva.extremes_transformer.transform(lower_bound_transformed)

        # Check and adjust upper bound
        # Upper bound should be within [actual_max, actual_max * 1.2]
        if upper_bound < actual_max or upper_bound > actual_max * 1.2:
            upper_bound = int(actual_max * 1.2)
        
        # Check and adjust lower bound
        # Special case: if actual_min is 0, lower bound should be 0
        if actual_min == 0:
            lower_bound = 0
        else:
            # Lower bound should be within [actual_min * 0.8, actual_min]
            if lower_bound > actual_min or lower_bound < actual_min * 0.8:
                lower_bound = int(actual_min * 0.8)

    return {
        'lower': int(lower_bound),
        'upper': int(upper_bound),
        'actual_min': actual_min,
        'actual_max': actual_max
    }

def analyzeBBTimeBounds(bb_time_map: dict, block_size: int = 100, alpha: float = 0.98) -> dict:
    result = {}
    
    for bbNum, data in bb_time_map.items():
        data_min = int(data.min())
        data_max = int(data.max())
        
        try:
            time_constraint = gevBasedTimeAnalysis(
                data=data,
                block_size=block_size,
                alpha=alpha
            )
            
            result[bbNum] = {
                'time_constraint': time_constraint
            }
             
        except Exception as e:
            # data_std = float(data.std())
            # data_unique = len(data.unique())
            
            # print(f"WARNING: bbNum {bbNum} GEV analysis failed, using actual min/max as bounds. "
            #       f"{str(e)[:60]}... [min={data_min}, max={data_max}, std={data_std:.2f}, unique_values={data_unique}]")
            
            # Use actual min/max as bounds when GEV analysis fails
            result[bbNum] = {
                'time_constraint': {
                    'lower': data_min,
                    'upper': data_max,
                    'actual_min': data_min,
                    'actual_max': data_max
                }
            }
    
    return result

def getTimeBounds(timeLogPath :str, block_size: int = 100, alpha: float = 0.98) -> dict:
    bb_time_map = parseLoggedTime(timeLogPath)
    INFO ("Start fitting GEV model...")
    filtered_bb_map = {bbNum: series for bbNum, series in bb_time_map.items() if len(series) > 1000}
    # INFO(f"Found {len(filtered_bb_map)} basic blocks with more than 1000 data points")
    timeConstraints = analyzeBBTimeBounds(filtered_bb_map, block_size=block_size, alpha=alpha)
    return timeConstraints

if __name__ == "__main__":
    bb_time_map = parseLoggedTime('/home/lqs66/Desktop/modelCheckingFlightControl/verifyDataBase/model_inputs/runtime_data/PX_RCFS_P1/PX4_P17_Time_18907.in')

    filtered_bb_map = {bbNum: series for bbNum, series in bb_time_map.items() if len(series) > 1000}
    
    print(f"\nFound {len(filtered_bb_map)} basic blocks with more than 1000 data points")
    
    if filtered_bb_map:
        timeConstraints = analyzeBBTimeBounds(filtered_bb_map, block_size=10, alpha=0.98)
        
        print("\n\n=== Final Results Summary ===")
        for bbNum in sorted(timeConstraints.keys()):
            tc = timeConstraints[bbNum]['time_constraint']
            if tc['lower'] is not None:
                print(f"bbNum {bbNum}: time_constraint: {{lower: {tc['lower']}, upper: {tc['upper']}, "
                      f"actual_min: {tc['actual_min']}, actual_max: {tc['actual_max']}}}")
            else:
                print(f"bbNum {bbNum}: time_constraint: {{lower: None, upper: None, actual_min: None, actual_max: None}}")
    else:
        print("\nNo basic blocks found with more than 1000 data points")
