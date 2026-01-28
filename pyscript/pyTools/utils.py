from colorama import Fore, Style, init
import os
from collections import OrderedDict
from dataclasses import dataclass, fields
from typing import Any, Tuple, Optional, Union

__all__ = ["DTMC" ,"ERROR", "WARNING", "INFO", "extractTimestamp", "IRConfig"]

DTMC = os.getenv('DTMC')

def ERROR(message):
    print(f"{Fore.RED}[ERROR]{Fore.RESET} {message}")

def WARNING(message):
    print(f"{Fore.YELLOW}[WARNING]{Fore.RESET} {message}")

def INFO(message):
    print(f"[INFO] {message}")

def extractTimestamp(file_path: str):
    '''
    Extract the timestamp from the file path.
    used for `property`_Global_`timestamp`.in, `property`_Heap_`timestamp`.in, and `property`_EntryArgs_`timestamp`.in.
    '''
    filename = os.path.basename(file_path)
    parts = filename.split('_')
    if len(parts) >= 3:
        timestamp_part = parts[-1]
        if timestamp_part.endswith('.in'):
            timestamp_part = timestamp_part[:-3]
        return timestamp_part
    return None

@dataclass()
class PythonStruct(OrderedDict):
    """
    An OrderedDict that can call its key by class attribute.
    Examples:
        @dataclass
        class SomeOutput(PythonStruct):
            OutputA: float
            OutputB: int = None

        output = SomeOutput(1.0)
        print(output.OutputA)  # will print 1.0, equivalent to od['OutputA']
    """
    def __post_init__(self):
        class_fields = fields(self)

        # Safety and consistency checks
        assert len(class_fields), f"{self.__class__.__name__} has no fields."
        # assert all(
        #     field.default is None for field in class_fields[1:]
        # ), f"{self.__class__.__name__} should not have more than one required field."

        for field in class_fields:
            v = getattr(self, field.name)
            if v is not None:
                self[field.name] = v

    def __delitem__(self, *args, **kwargs):
        raise Exception(f"You cannot use ``__delitem__`` on a {self.__class__.__name__} instance.")

    def setdefault(self, *args, **kwargs):
        raise Exception(f"You cannot use ``setdefault`` on a {self.__class__.__name__} instance.")

    def pop(self, *args, **kwargs):
        raise Exception(f"You cannot use ``pop`` on a {self.__class__.__name__} instance.")

    def update(self, *args, **kwargs):
        raise Exception(f"You cannot use ``update`` on a {self.__class__.__name__} instance.")

    def __getitem__(self, k):
        if isinstance(k, str):
            inner_dict = {k: v for (k, v) in self.items()}
            return inner_dict[k]
        else:
            return self.to_tuple()[k]

    def __setattr__(self, name, value):
        if name in self.keys() and value is not None:
            # Don't call self.__setitem__ to avoid recursion errors
            super().__setitem__(name, value)
        super().__setattr__(name, value)

    def __setitem__(self, key, value):
        # Will raise a KeyException if needed
        super().__setitem__(key, value)
        # Don't call self.__setattr__ to avoid recursion errors
        super().__setattr__(key, value)

    def to_tuple(self) -> Tuple[Any, ...]:
        """
        Convert self to a tuple containing all the attributes/keys that are not ``None``.
        """
        return tuple(self[k] for k in self.keys())


@dataclass
class IRConfig(PythonStruct):

    @dataclass
    class FuncParamValue(PythonStruct):
        id: int = None
        type: str = None
        value: str = None

    @dataclass
    class RegisterQuery(PythonStruct):
        pname: str = None
        cname: str = None
        file: str = None
        line: int = None
        col: int = None
        type: str = None

    @dataclass
    class StackQuery(PythonStruct):
        pname: str = None
        cname: str = None
        file: str = None
        line: int = None
        stackVarName: str = None
        offset: int = None
        type: str = None

    @dataclass
    class GlobalQuery(PythonStruct):
        pname: str = None
        cname: str = None
        globalVarName: str = None
        offset: int = None
        type: str = None

    property: str = None
    flightControl: str = None
    LLVM_IR: str = None
    call_depth: int = None
    entrypoints: Tuple[str, ...] = None
    params: Tuple[FuncParamValue, ...] = None
    isTime: bool = None
    registerQueries: Tuple[RegisterQuery, ...] = None
    stackQueries : Tuple[StackQuery, ...] = None
    globalQueries: Tuple[GlobalQuery, ...] = None
