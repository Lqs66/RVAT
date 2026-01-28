import typing
import abc

import numpy as np
import pandas as pd
import scipy.stats

from .distribution import Distribution


class AbstractModelBaseClass(abc.ABC):
    def __init__(
        self,
        extremes: pd.Series,
        distribution: typing.Union[str, scipy.stats.rv_continuous],
        distribution_kwargs: typing.Optional[dict] = None,
    ) -> None:
        """
        Initialize the model.

        Parameters
        ----------
        extremes : pandas.Series
            Series of extreme datas.
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

        """
        self.extremes = extremes

        # Declare extreme value distribution
        distribution_kwargs = distribution_kwargs or {}
        self.distribution = Distribution(
            extremes=self.extremes, distribution=distribution, **distribution_kwargs
        )
    
        # Fit the distribution to extremes
        self._fit_parameters: typing.Optional[dict] = None
        self._trace: typing.Optional[np.ndarray] = None
        self.fit()

    @property
    @abc.abstractmethod
    def name(self) -> str:
        """Return model name."""
        raise NotImplementedError
    
    @abc.abstractmethod
    def fit(self) -> None:
        """
        Set values for self.fit_parameters and self.trace.

        self.trace is set only for MCMC-like models.
        self.fit_parameters is a dictionary with {parameter_name: value},
        e.g. {'c': 0.1, 'loc': -7, 'scale': 0.3}
        self.trace is a numpy.ndarray with shape of
        (n_walkers, n_samples, n_free_parameters)

        """
        raise NotImplementedError
    

    @property
    def fit_parameters(self) -> typing.Dict[str, float]:
        if self._fit_parameters is None:
            raise AssertionError
        else:
            return self._fit_parameters

    @property
    def trace(self) -> np.ndarray:
        if self._trace is None:
            raise TypeError(f"trace property is not applicable for '{self.name}' model")
        else:
            return self._trace

    @property
    def loglikelihood(self) -> float:
        return np.sum(self.logpdf(x=self.extremes.values))
    
    @property
    def AIC(self) -> float:
        """
        Return corrected Akaike Information Criterion (AIC) of the model.

        Smaller AIC value corresponds to better model.
        This formula scales well for small sample sizes.
        See https://en.wikipedia.org/wiki/Akaike_information_criterion

        """
        k = self.distribution.number_of_parameters
        n = len(self.extremes)
        aic = 2 * k - 2 * self.loglikelihood
        if n <= k + 1:
            return aic
        correction = (2 * k**2 + 2 * k) / (n - k - 1)
        return aic + correction
    
    def _get_prop(self, prop: str, x):
        return self.distribution.get_prop(
            prop=prop, x=x, free_parameters=self.fit_parameters
        )

    def pdf(self, x):
        return self._get_prop(prop="pdf", x=x)

    def logpdf(self, x):
        return self._get_prop(prop="logpdf", x=x)

    def cdf(self, x):
        return self._get_prop(prop="cdf", x=x)

    def ppf(self, x):
        return self._get_prop(prop="ppf", x=x)

    def isf(self, x):
        return self._get_prop(prop="isf", x=x)