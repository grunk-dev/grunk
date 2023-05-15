from sys import platform

if platform == 'win32':
    from .plugin_manager import get_dll_paths
    from os import add_dll_directory

    #to do: configure version in setup step to fit the one given in conanfile.py
    for d in get_dll_paths("reflect", "0.1.4", in_grunk_dir=False):
        add_dll_directory(d)


from ._core import __version__ as _version
__version__ = _version
from ._core import *

from .plugin_helper import load
