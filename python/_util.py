import os
import sys
from grunk.plugin_manager import PluginManager
from conans.client.conan_api import ConanAPIV1
from conans.model.ref import ConanFileReference, PackageReference

class HiddenPrints:
    """This class is used to suppress output to stdout.

    Usage:

    .. code-block:: python

       with HiddenPrints():
           # put commands here
           pass
    """

    def __enter__(self):
        """
        redirects ``sys.stdout`` to ``os.devnull``
        """
        self._original_stdout = sys.stdout
        sys.stdout = open(os.devnull, "w")

    def __exit__(self, exc_type, exc_val, exc_tb):
        """
        resets ``sys.stdout`` to the original output
        """
        sys.stdout.close()
        sys.stdout = self._original_stdout


def deconstruct_package_string(p: str):
    """Given a string representing a grunk/conan package, this function
    deconstructs it to retrieve the package name, package version, user
    and channel.

    The string must be of the form
    ``<package_name>/<package_version>@<user>/<channel>``

    * ``/<package_version>`` is optional. If missing, ``version`` will be ``None``
    * ``@<user>/<channel>`` is optional. If missing, ``user`` and ``channel`` will be ``None``.
      But if there is an ``@`` in the string, both user and channel must be specified

    :param p: string representing a grunk/conan package
    :type p: str
    :raises ValueError: if the ``@`` delimiter splits the string into less than one or more than two parts
    :raises ValueError: if the substring before the first (or no) occurance of `@` is empty *(no package name and or version)*
    :raises ValueError: if the ``/`` delimiter splits the package_name/version part into less than one or more than two parts
    :raises ValueError: if the ``/`` delimiter splits the user/channel part into less than one or more than two parts
    :return: package_name, package_version, user, channel
    :rtype: tuple of strings
    """

    # TODO: maybe a regex is faster?

    s = p.split("@")
    if not len(s) in [1, 2]:
        raise ValueError(
            'Cannot parse package string "{}". Seperator "@" splits the string into too many substrings.'.format(
                p
            )
        )

    if s[0]:
        package_version = s[0]
    else:
        raise ValueError(
            'Cannot parse package/version in package string "{}"'.format(p)
        )

    pvs = package_version.split("/")
    package = pvs[0]

    if len(pvs) == 1:
        version = None
    elif len(pvs) == 2:
        version = pvs[1]
    else:
        raise ValueError(
            'Cannot parse package/version in package string "{}"'.format(p)
        )

    if len(s) == 2:
        ucs = s[1].split("/")
        if not len(ucs) == 2:
            raise ValueError(
                'Cannot parse user/channel in package string "{}"'.format(p)
            )
        user = ucs[0]
        channel = ucs[1]
    else:
        user = None
        channel = None

    return package, version, user, channel


def reconstruct_package_string(
    package: str, version: str = None, user: str = None, channel: str = None
):
    """
    reconstructs a conan/grunk package string of the form
    ```<package>/<version>@<user>/<channel>``` given the substrings package,
    version, user and channel.

    * If ``version`` is ``None``, the ``/<version>`` part will be missing
    * If ``user`` is ``None``, the ``@<user>/<channel> part will be missing
      If ``user`` is not ``None``, ``channel`` must also be not ``None``.


    :param package: substring for the package name
    :type package: str
    :param version: substring for the version, defaults to ``None``
    :type version: str
    :param user: substring for the user, defaults to ``None``
    :type user: str
    :param channel: substring for the channel, defaults to ``None``
    :type channel: str
    :return: the reconstructed conan/grunk package string
    :rtype: str
    """

    assert package is not None
    package_version = package

    if version is not None:
        package_version = package_version + "/" + version

    user_channel = ""
    if user is not None:
        assert channel is not None
        user_channel = "@{}/{}".format(user, channel)

    return "{}{}".format(package_version, user_channel)


def get_dll_paths(plugin_name, version, in_grunk_dir=True):
    """returns the shared library directories needed to load a given plugin

    :param plugin_name: The name of the plugin
    :type plugin_name: Str
    :param version: version string of the plugin
    :type version: Str
    :param in_grunk_dir: if set to true, grunk will search the .grunk directory rather than .conan, defaults to True
    :type in_grunk_dir: bool, optional
    :return: a list of directories
    :rtype: list of strings
    """
    ref = reconstruct_package_string(plugin_name, version)
    if not '@' in ref:
        ref = ref + '@_/_'

    dll_dir = 'lib'
    if sys.platform == 'win32':
        dll_dir = 'bin'

    def _get_dll_paths(package_ref, api):
        api.create_app()
        ref = ConanFileReference.loads(package_ref, validate=True)
        package_layout = api.app.cache.package_layout(ref, short_paths=None)

        deps_graph, _ = api.info(package_ref)
        package_dirs = []
        for node in deps_graph.nodes:
            if node.ref is not None and str(node.ref) in package_ref:
                pref = PackageReference(ref, node.package_id)
                dir = package_layout.package(pref)
                package_dirs.append(os.path.join(dir, dll_dir))

        return package_dirs

    if in_grunk_dir:
        with PluginManager() as pm:
            return _get_dll_paths(ref, pm._conan)
    else:
        return _get_dll_paths(ref, ConanAPIV1())
