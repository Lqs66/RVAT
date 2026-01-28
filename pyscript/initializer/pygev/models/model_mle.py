import itertools
import os
import typing
import multiprocessing

import numpy as np
import pandas as pd
import scipy.stats

from .model_base import AbstractModelBaseClass

class MLE(AbstractModelBaseClass):
    def __init__(
        self,
        extremes: pd.Series,
        distribution: typing.Union[str, scipy.stats.rv_continuous],
        distribution_kwargs: typing.Optional[dict] = None,
    ) -> None:
        """
        Maximum Likelihood Estimate (MLE) model.

        Built around the scipy.stats.rv_continuous.fit method.

        """
        super().__init__(
            extremes=extremes,
            distribution=distribution,
            distribution_kwargs=distribution_kwargs,
        )

    @property
    def name(self) -> str:
        return "MLE"
    
    def fit(self) -> None:
        self._fit_parameters = self.distribution.mle_parameters

    def __repr__(self) -> str:
        free_parameters = ", ".join(
            [
                f"{parameter}={self.fit_parameters[parameter]:.3f}"
                for parameter in self.distribution.free_parameters
            ]
        )

        fixed_parameters = ", ".join(
            [
                f"{key}={value:.3f}"
                for key, value in self.distribution.fixed_parameters.items()
            ]
        )
        if fixed_parameters == "":
            fixed_parameters = "all parameters are free"

        summary = [
            "MLE model",
            "",
            f"free parameters: {free_parameters}",
            f"fixed parameters: {fixed_parameters}",
            f"AIC: {self.AIC:.3f}",
            f"loglikelihood: {self.loglikelihood:.3f}",
        ]

        longest_row = max(map(len, summary))
        summary[1] = "-" * longest_row
        summary.append(summary[1])
        summary[0] = " " * ((longest_row - len(summary[0])) // 2) + summary[0]
        for i, row in enumerate(summary):
            summary[i] += " " * (longest_row - len(row))

        return "\n".join(summary)

