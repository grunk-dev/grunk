from .plugin_manager import PluginManager, get_dll_paths
from ._core import get_plugin_registry

def load(package_name: str,
        package_version: str = None,
        install_missing=False):
    if install_missing:
        with PluginManager() as pm:
            pm.install(package_name, package_version)
    for d in get_dll_paths(package_name, package_version):
        get_plugin_registry().prepend_path(d)