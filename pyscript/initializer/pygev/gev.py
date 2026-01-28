import typing
import warnings

import matplotlib.gridspec
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import scipy.stats

from .extremes import ExtremesTransformer, get_extremes
from .models import Distribution, MLE, get_model
from .plotting import (
    plot_extremes,
    pyextremes_rc,
)

class EVA:
    """
    Extreme Value Analysis (EVA) class.

    A typical workflow using the EVA class would consist of the following:
        - extract extreme values (.get_extremes)
        - fit a model (.fit_model)
        - generate outputs (.get_summary)
        - visualize the model (.plot_diagnostic, .plot_return_values)

    Multiple additional graphical and numerical methods are available
    within this class to analyze extracted extreme values, visualize them,
    assess goodness-of-fit of selected model, and to visualize its outputs.
    """

    __slots__ = [
        "__data",
        "__extremes",
        "__extremes_type",
        "__extremes_kwargs",
        "__extremes_transformer",
        "__model",
    ]

    __data: pd.Series
    __extremes: typing.Optional[pd.Series]
    __extremes_type: typing.Optional[typing.Literal["high", "low"]]
    __extremes_kwargs: typing.Optional[typing.Dict[str, typing.Any]]
    __extremes_transformer: typing.Optional[ExtremesTransformer]
    __model: MLE

    def __init__(self, data: pd.Series) -> None:
        """
        Initialize EVA model.

        Parameters
        ----------
        data : pandas.Series
            one-dimensional sequence.
            values must be numeric.
        """
        if not isinstance(data, pd.Series):
            raise TypeError(
                f"invalid type in '{type(data).__name__}' for the `data` argument, "
                f"must be pandas.Series"
            )
        # Copy `data` to ensure the original Series object it is not mutated
        data = data.copy(deep=True)

        # Ensure that `data` has correct index and value dtypes
        if not np.issubdtype(data.dtype, np.number):
            try:
                message = "`data` values are not numeric - converting to numeric"
                warnings.warn(message=message, category=RuntimeWarning)
                data = data.astype(np.float64)
            except ValueError as _error:
                raise TypeError(
                    f"invalid dtype in {data.dtype} for the `data` argument, "
                    f"must be numeric (subdtype of numpy.number)"
                ) from _error
            
        # Ensure that `data` has no invalid entries
        # we filter out NaNs and Infs
        # data = data.dropna()
        invalidNums = len(data[~np.isfinite(data)])
        data = data[np.isfinite(data)]
        if invalidNums > 0:
            message = f"`data` contains {invalidNums} non-finite values - filtering out"
            warnings.warn(message=message, category=RuntimeWarning)

        self.__data: pd.Series = data
        self.__extremes = None
        self.__extremes_type = None
        self.__extremes_kwargs = None
        self.__extremes_transformer = None

        # Initialize attributes related to model fitting
        self.__model = None

    @property
    def data(self) -> pd.Series:
        return self.__data
    
    @property
    def extremes(self) -> pd.Series:
        if self.__extremes is None:
            raise AttributeError(
                "extreme values must first be extracted "
                "using the '.get_extremes' method"
            )
        return self.__extremes
        
    @property
    def extremes_type(self) -> typing.Literal["high", "low"]:
        if self.__extremes_type is None:
            raise AttributeError(
                "extreme values must first be extracted "
                "using the '.get_extremes' method"
            )
        return self.__extremes_type  
    
    @property
    def extremes_kwargs(self) -> typing.Dict[str, typing.Any]:
        if self.__extremes_kwargs is None:
            raise AttributeError(
                "extreme values must first be extracted "
                "using the '.get_extremes' method"
            )
        return self.__extremes_kwargs

    @property
    def extremes_transformer(self) -> ExtremesTransformer:
        if self.__extremes_transformer is None:
            raise AttributeError(
                "extreme values must first be extracted "
                "using the '.get_extremes' method"
            )
        return self.__extremes_transformer    

    @property
    def model(self) -> MLE:
        if self.__model is None:
            raise AttributeError(
                "model must first be assigned using the '.fit_model' method"
            )
        return self.__model

    @property
    def distribution(self) -> Distribution:
        return self.model.distribution

    @property
    def loglikelihood(self) -> float:
        return self.model.loglikelihood

    @property
    def AIC(self) -> float:
        return self.model.AIC
    
    def get_extremes(
        self,
        extremes_type: typing.Literal["high", "low"] = "high",
        block_size: typing.Optional[int] = 100,
        min_last_block: typing.Optional[float] = None,
    ) -> None:
        """
        Get extreme events from time series.

        Parameters
        ----------
        datas : pandas.Series
            Series of user input data.
        extremes_type : str, optional
            high (default) - get extreme high values
            low - get extreme low values
        block_size : int optional
            Block size (default=100).
        min_last_block : float, optional
            Minimum data availability ratio (0 to 1) in the last block
            for it to be used to extract extreme value from.
            This is used to discard last block when it is too short.
            If None (default), last block is always used.

        Returns
        -------
        extremes : pandas.Series

        """
        self.__extremes = get_extremes(
            datas=self.data,
            extremes_type=extremes_type,
            block_size=block_size,
            min_last_block=min_last_block,
        )
        self.__extremes_type = extremes_type
        self.__extremes_kwargs = {'block_size': block_size, 'min_last_block': min_last_block}

        self.__extremes_transformer = ExtremesTransformer(
            extremes=self.__extremes,
            extremes_type=self.__extremes_type,
        )        
        self.__model = None

    def plot_extremes(
        self,
        figsize: tuple = (8, 5),
        ax: typing.Optional[plt.Axes] = None,
    ) -> typing.Tuple[plt.Figure, plt.Axes]:  # pragma: no cover
        """
        Plot extreme events.

        Parameters
        ----------
        figsize : tuple, optional
            Figure size in inches in format (width, height).
            By default it is (8, 5).
        ax : matplotlib.axes._axes.Axes, optional
            Axes onto which extremes plot is drawn.
            If None (default), a new figure and axes objects are created.
        show_clusters : bool, optional
            If True, show cluster boundaries for POT extremes.
            Has no effect if extremes were extracted using BM method.
            May produce wrong cluster boundaries if extremes were set using the
            `set_extremes` or `from_extremes` methods and threshold and inter-cluster
            distance (r) arguments were not provided.
            By default is False.

        Returns
        -------
        figure : matplotlib.figure.Figure
            Figure object.
        axes : matplotlib.axes._axes.Axes
            Axes object.

        """
        return plot_extremes(
            datas=self.data,
            extremes=self.extremes,
            extremes_type=self.extremes_type,
            block_size=self.extremes_kwargs.get("block_size", None),
            figsize=figsize,
            ax=ax,
        )
    
    def fit_model(
        self,
        distribution: typing.Union[str, scipy.stats.rv_continuous] = None,
        distribution_kwargs: typing.Optional[dict] = None,
    ) -> None:
        """
        Fit a model to the extracted extreme values.

        Parameters
        ----------
        distribution : str or scipy.stats.rv_continuous, optional
            Distribution name compatible with scipy.stats
            or a subclass of scipy.stats.rv_continuous.
            See https://docs.scipy.org/doc/scipy/reference/stats.html
            By default the distribution is selected automatically
            as best between 'genextreme' and 'gumbel_r' for 'BM' extremes.
            Best distribution is selected using the AIC metric.
        distribution_kwargs : dict, optional
            Special keyword arguments, passed to the `.fit` method of the distribution.
            These keyword arguments represent parameters to be held fixed.
            Names of parameters to be fixed must have 'f' prefixes. Valid parameters:
                - shape(s): 'fc', e.g. fc=0
                - location: 'floc', e.g. floc=0
                - scale: 'fscale', e.g. fscale=1
            See documentation of a specific scipy.stats distribution
            for names of available parameters.
            By default, location parameter for 'genpareto' and 'expon' distributions
            is fixed to threshold (POT) or to minimum extremes (BM) value.
            Set to empty dictionary (distribution_kwargs={}) to avoid this behaviour.
        """  
        if distribution is None:
            candidate_distributions = ["genextreme", "gumbel_r"]
            _distribution_kwargs = None
            distribution = None
            aic = np.inf
            for distribution_name in candidate_distributions:
                new_aic = MLE(
                    extremes=self.extremes_transformer.transformed_extremes,
                    distribution=distribution_name,
                    distribution_kwargs=_distribution_kwargs,
                ).AIC
                if new_aic < aic:
                    distribution = distribution_name
                    aic = new_aic
        
        # Get distribution name
        if isinstance(distribution, str):
            distribution_name = distribution
        elif isinstance(distribution, scipy.stats.rv_continuous):
            distribution_name = getattr(distribution, "name", None)
        else:
            raise TypeError(
                f"invalid type in {type(distribution)} "
                f"for the 'distribution' argument, "
                f"must be string or scipy.stats.rv_continuous"
            )
        
        # Fit model to transformed extremes
        self.__model = get_model(
            extremes=self.extremes_transformer.transformed_extremes,
            distribution=distribution,
            distribution_kwargs=distribution_kwargs,
        )
    
    def plot_pdf(
        self,
        alpha: typing.Optional[float] = None,
        plotting_position: typing.Literal[
            "ecdf",
            "hazen",
            "weibull",
            "tukey",
            "blom",
            "median",
            "cunnane",
            "gringorten",
            "beard",
        ] = "weibull",
        figsize: typing.Tuple[float, float] = (8, 8),
        n_samples: typing.Optional[int] = 100,
    ):  # pragma: no cover
        """
        Plot a probability density (PDF) plot plot.

        Parameters
        ----------
        alpha : float, optional
            Width of confidence interval (0, 1).
            If None (default), confidence interval bounds are not plotted.
        plotting_position : str, optional
            Plotting position name (default='weibull'), not case-sensitive.
            Supported plotting positions:
                ecdf, hazen, weibull, tukey, blom, median, cunnane, gringorten, beard
        figsize : tuple, optional
            Figure size in inches in format (width, height).
            By default it is (8, 8).
        n_samples : int, optional
            Number of bootstrap samples used to estimate
            confidence interval bounds (default=100).

        Returns
        -------
        figure : matplotlib.figure.Figure
            Figure object.
        axes : tuple
            Tuple with four Axes objects: pdf

        """
        with plt.rc_context(rc=pyextremes_rc):
            fig, ax = plt.subplots(figsize=figsize, dpi=96)
            # Plot PDF
            ax_pdf = ax
            pdf_support = np.linspace(self.extremes.min(), self.extremes.max(), 100)
            pdf = self.model.pdf(self.extremes_transformer.transform(pdf_support))
            ax_pdf.grid(False)
            ax_pdf.set_title("Probability density plot")
            ax_pdf.set_ylabel("Probability density")
            ax_pdf.set_xlabel(self.data.name)
            ax_pdf.hist(
                self.extremes.values,
                bins=np.histogram_bin_edges(a=self.extremes.values, bins="auto"),
                density=True,
                rwidth=0.8,
                facecolor="#5199FF",
                edgecolor="None",
                lw=0,
                alpha=0.25,
                zorder=5,
            )
            ax_pdf.hist(
                self.extremes.values,
                bins=np.histogram_bin_edges(a=self.extremes.values, bins="auto"),
                density=True,
                rwidth=0.8,
                facecolor="None",
                edgecolor="#5199FF",
                lw=1,
                ls="--",
                zorder=10,
            )
            ax_pdf.plot(pdf_support, pdf, color="#F85C50", lw=2, ls="-", zorder=15)
            ax_pdf.scatter(
                self.extremes.values,
                np.full(shape=len(self.extremes), fill_value=0),
                marker="|",
                s=40,
                color="k",
                lw=0.5,
                zorder=15,
            )
            ax_pdf.set_ylim(0, ax_pdf.get_ylim()[1])
            return fig, ax_pdf