from ._core import __version__ as _version
__version__ = _version
from ._core import *

from ._core.reflect import help as reflect_help
def help(pattern):
    print(reflect_help(pattern))

from .plugin_helper import load
