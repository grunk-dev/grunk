import os
import tempfile
from sys import platform
from pathlib import Path
from functools import wraps
from conans.client.conan_api import ConanAPIV1
from conans.model.ref import ConanFileReference, PackageReference
from grunk._util import HiddenPrints, reconstruct_package_string


def is_installed(package_name, version):

    with PluginManager() as pm:
        package_ref = f"{package_name}/{version}"

        # first search recipes
        result = pm._conan.search_recipes(package_ref)
        if result['error']:
            raise RuntimeError("Error searching for recipes")
        if len(result['results']) == 0:
            return False

        # next search packages
        result = pm._conan.search_packages(reference=package_ref, remote_name=None)
        if result['error']:
            raise RuntimeError("Error searching for packages")
        return len(result['results']) > 0


def get_latest_package_version(package_name, api):
    # Get the package reference for the specified package name
    package_ref = ConanFileReference.loads(package_name, validate=False)
    res = api.search_recipes(package_ref.name)
    assert(not res['error']) #TODO: Better error handling here
    ids = []
    for r in [result for result in res['results']]:
        for i in r['items']:
            ids.append(i['recipe']['id'])
    version_numbers = [id.split("/")[1] for id in ids]
    return max(version_numbers)

def get_dll_paths(plugin_name, version = None, in_grunk_dir=True):
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

    dll_dir = 'lib'
    if platform == 'win32':
        dll_dir = 'bin'

    def _get_dll_paths(plugin_name, version, api):

        if version is None:
            version = get_latest_package_version(plugin_name, api)
        package_ref = f"{plugin_name}/{version}@_/_"

        api.create_app()
        ref = ConanFileReference.loads(package_ref, validate=False)
        package_layout = api.app.cache.package_layout(ref, short_paths=None)

        deps_graph, _ = api.info(package_ref)
        package_dirs = []
        for node in deps_graph.nodes:
            if node.ref is not None and str(node.ref) in package_ref:
                pref = PackageReference(ref, node.package_id)
                prefix = package_layout.package(pref)
                d = os.path.join(prefix, dll_dir)
                if os.path.isdir(d):
                    package_dirs.append(d)

        return package_dirs

    if in_grunk_dir:
        with PluginManager() as pm:
            return _get_dll_paths(plugin_name, version, pm._conan)
    else:
        return _get_dll_paths(plugin_name, version, ConanAPIV1())


class PluginManager:
    """
    The PluginManager class is used to setup the conan directory and some conan variables and settings.
    It stores a reference to the Conan API. At __exit__(), the conan directory is reset.

    It is recommended to use PluginManager in combination with a with statement, e.g.

    .. code-block:: python

       with PluginManager() as pm:
           pm.install(package_name)
    """

    def __init__(self):
        """
        Changes the conan user home to <HOME>/.grunk so that grunk doesn't interfere with an
        existing conan setup and

        * adds grunk_center to the remotes
        * sets the default user and channel to "_/_"
        * sets up the default profile. On Linux with GCC>5 it sets  "settings.compiler.libcxx" to "libstdc++11"
        """

        # set default variables
        self.grunk_dir = os.path.join(str(Path.home()), ".grunk")
        self.remote = "grunk_center"
        self.remote_url = "https://gitlab.dlr.de/api/v4/projects/21487/packages/conan"
        self.default_user = "_"
        self.default_channel = "_"

        # store previous CONAN_USER_HOME so that it can be reset and update CONAN_USER_HOME
        self.conan_user_home_prev = os.environ.get("CONAN_USER_HOME")
        os.environ["CONAN_USER_HOME"] = os.getenv("GRUNK_USER_HOME", self.grunk_dir)

        self._conan = ConanAPIV1()

        # add PluginManager remote
        if not [
            entry
            for entry in self._conan.remote_list()
            if (entry.name == self.remote and entry.url == self.remote_url)
        ]:
            self._conan.remote_add(self.remote, self.remote_url)

        # update compiler.libcxx in default profile
        if "default" not in self._conan.profile_list():
            with HiddenPrints():  # suppress print statements. conan warns about wrong libcxx when creating the profile
                self._conan.create_profile("default", detect=True)

        s = self._conan.read_profile("default").settings

        # update libcxx for GCC>=5
        if (
            "compiler" in s
            and s["compiler"] == "gcc"
            and "compiler.version" in s
            and int(s["compiler.version"]) >= 5
        ):
            if "compiler.libcxx" in s:

                libcxx = s["compiler.libcxx"]

                def get_gcc_version():
                    version_str = os.popen("gcc --version").read()
                    first_line = version_str.split("\n")[0]
                    return first_line.rsplit(" ", 1)[-1]

                gcc_version = get_gcc_version()
                gcc_major = int(gcc_version.split(".")[0])

                if gcc_major > 5 and not libcxx == "libstdc++11":
                    self._conan.update_profile("default", "settings.compiler.libcxx", "libstdc++11")

    def __enter__(self):
        """
        for use in combination in a with statement. Returns a reference to self
        """
        return self

    def __exit__(self, *exc):
        """
        resets the CONAN_USER_HOME
        """
        if self.conan_user_home_prev is None:
            del os.environ["CONAN_USER_HOME"]
        else:
            os.environ["CONAN_USER_HOME"] = self.conan_user_home_prev

    def install(
        self,
        package_name: str,
        package_version: str = None,
        user: str = None,
        channel: str = None,
        install_dir: str = None,
        update = False
    ):
        """
        installs a package reference.

        :param package_name: The name of the package to be installed
        :type package_name: str
        :param package_version: The version of the package to be installed. If not specified, grunk tries
                                to install the latest version. This requires the package to adhere to semver2
                                versioning. conan version ranges are supported, defaults to ``None``
        :type package_version: str, optional
        :param user: user string for the package, defaults to ``"paradigms"``
        :type user: str, optional
        :param channel: channel string for the package, defaults to ``"testing"``
        :type channel: str, optional
        :param install_dir: Installation directory, defaults to ``<HOME>/.grunk``
        :type install_dir: str, optional

        """

        package_str = self._get_package_ref(package_name, package_version, user, channel)

        ref = ConanFileReference.loads(package_str, validate=False)

        self._conan.install_reference(
            ref,
            install_folder=install_dir,
            generators=["deploy"],
            # remote_name=self.remote,
            build=["missing"],
            update=update,
        )

    def virtualrunenv(
        self,
        package_refs,
    ):
        """
        creates scripts to activate/deactivate a virtual run environment 
        for the package references (strings) defined in package reFs

        :param package_refs: list of package_ref (name/version)
        """

        tmp = tempfile.NamedTemporaryFile(mode = "w", delete=False)
        try:
            tmp.write("[requires]\n")
            for ref in package_refs:
                tmp.write(ref + "\n")
            tmp.close()
            self._conan.install(
                tmp.name,
                generators=["virtualrunenv"],
            )
        finally:
            os.unlink(tmp.name)

    
    def _get_package_ref(self, package_name, package_version, user, channel):
        # To Do: It would be nice to support installation from conancenter. Then we wouldn't
        # want to use the default_user and default_channel here
        if user is None:
            user = self.default_user
        if channel is None:
            channel = self.default_channel
        if install_dir is None:
            install_dir = self.grunk_dir

        if package_version is None:
            # The version ranges aren't searched for in the remotes https://github.com/conan-io/conan/issues/3113
            # This can be fixed once we migrate to conan 2.0
            raise RuntimeError("As of now, a version must explicitly be specified.")
            package_version = "[>0.0.1]"

        return reconstruct_package_string(
            package_name, package_version, user, channel
        )



    def authenticate(
        self, user: str, password: str, remote_name: str, skip_auth: bool = False
    ):
        """
        authenticates a user with a password for a given remote.

        This is needed for remotes with restricted access rights.
        The authentification is not persistent. grunk obtains an
        authentification token from the server with an expiration
        date set by the server.

        The password is stored in a local database using a very
        basic *(meaning not very secure)* level of encryption.

        :param user: user name
        :type user: str
        :param password: password for the user
        :type password: str
        :param remote_name: name of the remote
        :type remote_name: str
        :param skip_auth: skips authentification with the server
                          if there are local stored credentials,
                          defaults to False
        :type skip_auth: bool, optional
        :return: remote_name, prev_user, user
        :rtype: tuple of strings
        """
        return self._conan.authenticate(user, password, remote_name, skip_auth)


    def remove(self, pattern):
        """
        removes plugin(s) matching a given pattern

        :param pattern: All packages matching this pattern will be removed
        """

        #To Do: Provide more options
        return self._conan.remove(pattern, query=None, packages=None, builds=None, src=False, force=False,
               remote_name=None, outdated=False)


    def list(self):
        result = self._conan.search_recipes('')
        if result['error']:
            raise RuntimeError("Error generating list of packages")
        packages = []
        for d in result['results']:
            for i in d['items']:
                packages.append(i['recipe']['id'])
        return packages

def command(f):
    """Decorator for PluginManager methods to be used as free functions

    It wraps the method call in a with statement, so that PluginManager is
    setup properly before the call and all variables are reset
    after the call.

    :param f: A PluginManager method or a function accepting a PluginManager instance as
              first argument
    :type f: callable
    :return: A function accepting the same arguments as the PluginManager method, without the
             first argument (``self``).
    :rtype: callable

    """

    @wraps(f)
    def wrapper(*args, **kwargs):
        with PluginManager() as g:
            return f(g, *args, **kwargs)

    return wrapper


# decorate PluginManager methods
virtualrunenv = command(PluginManager.virtualrunenv)
install = command(PluginManager.install)
authenticate = command(PluginManager.authenticate)
remove = command(PluginManager.remove)
avail = command(PluginManager.list)
