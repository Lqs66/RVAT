from typing import Literal, Optional, Union, overload

import pandas as pd
import scipy.stats

from .model_mle import MLE

def get_model(
    extremes: pd.Series,
    distribution: Union[str, scipy.stats.rv_continuous],
    distribution_kwargs: Optional[dict] = None,
) -> MLE:
    """
    Get distribution fitting model and fit it to given extreme values.

    Parameters
    ----------
    extremes : pandas.Series
        Time series of extreme events.
    distribution : str or scipy.stats.rv_continuous
        Distribution name compatible with scipy.stats
        or a subclass of scipy.stats.rv_continuous.
        See https://docs.scipy.org/doc/scipy/reference/stats.html
    distribution_kwargs : dict, optional
        Special keyword arguments, passed to the `.fit` method of the distribution.
        These keyword arguments represent parameters to be held fixed.
        Names of parameters to be fixed must have 'f' prefixes. Valid parameters:
            - shape(s): 'fc', e.g. fc=0
            - location: 'floc', e.g. floc=0
            - scale: 'fscale', e.g. fscale=1
        By default, no parameters are fixed.
        See documentation of a specific scipy.stats distribution
        for names of available parameters.

    Returns
    -------
    model : MLE or Emcee
        Distribution fitting model fitted to the `extremes`.

    """
    distribution_model_kwargs = {
        "extremes": extremes,
        "distribution": distribution,
        "distribution_kwargs": distribution_kwargs,
    }

    return MLE(**distribution_model_kwargs)
