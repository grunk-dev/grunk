"""_summary_ includes helper functions for the package manager, making use of the 
   python bindings in ._core
"""

from .plugin_manager import PluginManager, get_dll_paths, is_installed
from grunk import get_plugin_registry

def load(package_name: str,
        version: str = None,
        install_missing=False):
    plugins = get_plugin_registry()
    if install_missing and not is_installed(package_name, version):
        with PluginManager() as pm:
            pm.install(package_name, version)
    for d in get_dll_paths(package_name, version):
        plugins.prepend_path(d)
    plugins.load_all()
