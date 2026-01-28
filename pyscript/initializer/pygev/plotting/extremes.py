from typing import Any, Literal, Optional, Tuple, Union

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

from .style import pyextremes_rc

def plot_extremes(
    datas: pd.Series,
    extremes: pd.Series,
    extremes_type: Optional[Literal["high", "low"]] = None,
    block_size: int = 100,
    figsize: Tuple[float, float] = (8, 5),
    ax: Optional[plt.Axes] = None,
) -> Tuple[plt.Figure, plt.Axes]:
    """
    Plot extreme events.

    Parameters
    ----------
    datas : pandas.Series
        Raw datas.
    extremes : pandas.Series
        Series of extreme datas.
    extremes_type : str, optional
        Type of `extremes`, used only if `extremes_method` is 'POT'
        and `threshold` is not provided.
            high - extreme high values
            low - get low values
    block_size : int
        Block size (default=100).
    figsize : tuple, optional
        Figure size in inches in format (width, height).
        By default it is (8, 5).
    ax : matplotlib.axes._axes.Axes, optional
        Axes onto which extremes plot is drawn.
        If None (default), a new figure and axes objects are created.

    Returns
    -------
    figure : matplotlib.figure.Figure
        Figure object.
    axes : matplotlib.axes._axes.Axes
        Axes object.

    """
    if extremes_type not in ["high", "low"]:
        raise ValueError(
            f"invalid value in '{extremes_type}' for the 'extremes_type' argument"
        )
    
    with plt.rc_context(rc=pyextremes_rc):
        # Create figure
        if ax is None:
            fig, ax = plt.subplots(figsize=figsize, dpi=96)
        else:
            try:
                fig = ax.get_figure()
            except AttributeError as _error:
                raise TypeError(
                    f"invalid type in {type(ax)} for the 'ax' argument, "
                    f"must be matplotlib Axes object"
                ) from _error

        # Configure axes
        ax.grid(False)

        num_blocks = len(datas) // block_size + 1
        for i in range(num_blocks):
            start = i * block_size
            end = min((i + 1) * block_size, len(datas))
            ax.plot(
                range(start, end), 
                datas[start:end], 
                color="#5199FF", lw=1, zorder=10
            )

        # for i in range(num_blocks):
        #     start = i * block_size
        #     end = min((i + 1) * block_size, len(datas))
            
        #     for j in range(start, end):
        #         ax.vlines(
        #             x=j, 
        #             ymin=0, 
        #             ymax=datas[j], 
        #             color="#5199FF", 
        #             lw=0.5, 
        #             zorder=10
        #         )
        
        for i in range(0, num_blocks+1):
            ax.axvline(i * block_size, color="#5199FF", lw=0.3, ls=":", zorder=10)

        ax.scatter(
            extremes.index,
            extremes.values,
            s=15,
            lw=0.5,
            edgecolor="w",
            facecolor="#F85C50",
            zorder=20,
        )
        
        tick_interval = len(datas) // 5
        xticks = range(0, len(datas) + tick_interval, tick_interval)
        ax.set_xticks(xticks)
        ax.set_xticklabels(xticks)
        
        ax.set_xlabel("Index")
        ax.set_ylabel("Value")
        ax.legend()

        return fig, ax