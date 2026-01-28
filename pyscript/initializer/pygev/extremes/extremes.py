from typing import Literal, Optional, overload
import pandas as pd

from .block_maxima import get_extremes_block_maxima

def get_extremes(
    datas: pd.Series,
    extremes_type: Literal["high", "low"] = "high",
    block_size: Optional[int] = 100,
    min_last_block: Optional[float] = None,
) -> pd.Series:
    """
    Get extreme events from time series.

    Parameters
    ----------
    datas : pandas.Series
        Series of user input data.
    extremes_type : str, optional
        high (default) - get extreme high values
        low - get extreme low values
    block_size : str or pandas.Timedelta, optional
        Block size (default='365.2425D').
    min_last_block : float, optional
        Minimum data availability ratio (0 to 1) in the last block
        for it to be used to extract extreme value from.
        This is used to discard last block when it is too short.
        If None (default), last block is always used.

    Returns
    -------
    extremes : pandas.Series
        Time series of extreme events.

    """
    return get_extremes_block_maxima(
        datas=datas,
        extremes_type=extremes_type,
        block_size=block_size,
        min_last_block=min_last_block,
    )