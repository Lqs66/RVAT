from typing import Literal, Optional
import warnings
import numpy as np
import pandas as pd

def get_extremes_block_maxima(
    datas: pd.Series,
    extremes_type: Literal["high", "low"],
    block_size: Optional[int] = 100,
    min_last_block: Optional[float] = None,
) -> pd.Series:
    """
    Get extreme events from time series using the Block Maxima/Minima method.

    Parameters
    ----------
    datas : pandas.Series
        Series of user input data.
    block_size : int
        Block size (default=100).
    min_last_block : float, optional
        Minimum data availability ratio (0 to 1) in the last block
        for it to be used to extract extreme value from.
        This is used to discard last block when it is too short.
        If None (default), last block is always used.

    Returns
    -------
    extremes : pandas.Series
        Series of block maxima/Minima extreme values.

    """

    if extremes_type not in ["high", "low"]:
        raise ValueError(
            f"invalid value in '{extremes_type}' for the 'extremes_type' argument"
        )
    
    # Get extreme value extraction function
    if extremes_type == "high":
        extremes_func = pd.Series.idxmax # Get max value's index
        retName = "block maxima"
    else:
        extremes_func = pd.Series.idxmin # Get min value's index
        retName = "block minima"

    extreme_values = []
    extreme_indices = []
    for i in range(0, len(datas), block_size):
        block = datas.iloc[i:i + block_size]
        if len(block) == 0:
            # Block is empty
            warnings.warn("Block is empty, applying NaN")
            extreme_values.append(np.nan)
            extreme_indices.append(datas.index[i])
        else:
            extreme_index = extremes_func(block)
            extreme_values.append(datas.loc[extreme_index])
            extreme_indices.append(extreme_index)

    if min_last_block is not None:
        ratio = len(datas) % block_size / block_size
        if ratio < min_last_block:
            extreme_values = extreme_values[:-1]
            extreme_indices = extreme_indices[:-1]

    return pd.Series(
        data=extreme_values,
        index=extreme_indices,
        dtype=np.float64,
        name=retName,
    ).fillna(np.nanmean(extreme_values)) # Fill NaNs with mean of all other extreme events in the series