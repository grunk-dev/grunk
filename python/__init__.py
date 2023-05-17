from sys import platform

if platform == 'win32':
    from .plugin_manager import get_dll_paths
    from os import add_dll_directory

    #to do: configure reflect version in setup step to fit the one given in conanfile.py
    #Currently we are taking the latest one (which should be fine under normal circumstances)
    for d in get_dll_paths("reflect", in_grunk_dir=False):
        add_dll_directory(d)


from ._core import __version__ as _version
__version__ = _version
from ._core import *

from ._core.reflect import help as reflect_help
def help(pattern):
    print(reflect_help(pattern))

from .plugin_helper import load
