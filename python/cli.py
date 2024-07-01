"""Console script for grunk."""

import os
import sys
import yaml

import click

import grunk
import grunk.plugin_manager as pm
from grunk._util import deconstruct_package_string
from grunk.codegen import generate
from ._core import get_plugin_registry
from conans.errors import ConanException


@click.group()
@click.version_option(grunk.__version__)
def cli():
    """
    command line interface for grunk.
    """
    pass


@cli.command()
@click.argument("packages", nargs=-1)
@click.option("-s", "--settings", type=str, multiple=True, help="settings for the packages")
@click.option("-o", "--options", type=str, multiple=True, help="options for the packages")
def install(packages, settings, options):
    """
    install grunk plugins.

    Accepts a list of package references as an argument. These are of the form
    <package_name> or <package_name>/<package_version> or <package_name>/<package_version>@<user>/<channel>,
    see also the conan documentation.

    The version part can be a string or a version range. If no version is specified grunk installs the latest version.

    You may ommit user/channel for packages in the grunkcenter.

    Optionally you install the packages using settings passed via -s/--settings. These settings will be
    forwarded to conan.

    Optionally you install the packages using options passed via -o/--options. These settings will be
    forwarded to conan.

    Example:

       grunk install PluginA reflect/[>0.0.1] myplugin/2.0@ford_prefect/release -s build_type=Debug
    """
    for package_str in packages:
        package, version, user, channel = deconstruct_package_string(package_str)
        try:
            pm.install(
                package_name=package,
                package_version=version,
                user=user,
                channel=channel,
                settings=list(settings),
                options=list(options)
            )
        except ConanException:
            print(
                'Could not find package "' + package_str + '" in remotes.'
            )  # TODO: logging

@cli.group()
def env():
    """
    interact with isolated grunk environments
    """
    pass

@env.command()
@click.argument("environment_name")
@click.argument("packages", nargs=-1)
def create(environment_name, packages):
    """
    Creates a new isolated environment

    This environment contains all runtime dependencies of 
    the specified packages/plugins.

    Accepts a list of package references as an argument. These are of the form
    <package_name> or <package_name>/<package_version> or <package_name>/<package_version>@<user>/<channel>,
    see also the conan documentation.

    The version part can be a string or a version range. If no version is specified grunk installs the latest version.

    You may ommit user/channel for packages in the grunkcenter.

    Example:

       grunk env create my_env myplugin/2.0@ford_prefect/release yourplugin/1.2.0
    """
    return pm.env_create(environment_name, packages)


@env.command(name="list") # need to use an alias because I cannot name the function "list" without overriding built-in function "list"
def list_command():
    """prints a list of all environments
    """
    return pm.env_list()


@env.command()
@click.argument("environment_name")
def show(environment_name):
    """prints the plugins installed in an environment
    """
    return pm.env_show(environment_name)


@env.command()
@click.argument("environment_name")
def remove(environment_name):
    """removes an environment
    """
    return pm.env_remove(environment_name)


@cli.group()
def user():
    """
    interact with the authentification system of remote servers
    """
    pass


# TODO
# @user.command()
# def clean():
#     """
#     remove user and tokens for all remotes
#     """
#     pass


# @user.command()
# @click.option(
#     "-r", "--remote", "string", help="lists users of a specific remote server"
# )
# def list():
#     """
#     list users (of a remote)
#     """
#     pass


@user.command()
@click.argument("USER_NAME")
@click.option(
    "-p", "--password", help="Password for authentification with remote", default=None
)
@click.option(
    "-r", "--remote", help="remote server", default="grunkcenter", show_default=True
)
@click.option(
    "-s",
    "--skip-auth",
    help="Skips the authentification with the server if there are local stored credentials. It doesn't check if the current credentials are valid or not.",
    default=False,
    is_flag=True,
)
def auth(user_name, password, remote, skip_auth):
    """
    authenticate with a remote server. Pass the user name as the first positional argument.
    """
    # TODO: wrap in try block. Catch e.g. NoRemoteAvailable
    return pm.authenticate(user_name, password, remote, skip_auth)


@cli.command()
@click.argument("pattern")
def remove(pattern):
    """
    remove plugins matching a given pattern from the local cache.
    """
    return pm.remove(pattern)

@cli.command()
def avail():
    """
    Lists packages installed in the local cache

    This includes both grunk plugins and their dependencies.
    """
    packages = pm.avail()
    packages.sort()
    for p in packages:
        print(p)


@cli.command()
@click.argument("grunk_recipe", type=click.Path(exists=True))
@click.option("-e", "--environment", help="name of a grunk environment", type=str)
def exec(grunk_recipe, environment=None):
    """evaluates all features of a grunk recipe
    """

    # we need to tell pyyaml to ignore our tags
    # https://stackoverflow.com/questions/33048540/pyyaml-safe-load-how-to-ignore-local-tags
    class SafeLoaderIgnoreUnknown(yaml.SafeLoader):
        def ignore_unknown(self, node):
            return None 
    SafeLoaderIgnoreUnknown.add_constructor(None, SafeLoaderIgnoreUnknown.ignore_unknown)

    # parse recipe with pyyaml to parse plugins that need to be loaded
    with open(grunk_recipe, "r")  as file:
        recipe = yaml.load(file, Loader=SafeLoaderIgnoreUnknown)

    if environment is not None:
        grunk.get_plugin_registry().activate_env(environment)
    
    for name, version in recipe['uses'].items():
        if not name == 'grunk':
            grunk.get_plugin_registry().load(name)

    # parse recipe with grunk and evaluate all nodes
    nodes = grunk.read(grunk_recipe)
    for n in nodes.get_features().values():
        n.value()


@cli.command()
@click.argument("config_file",  type=click.Path(exists=True))
@click.option("-o", "--output-dir", help="output directory for generated source files",  type=click.Path(exists=True))
@click.option("-i", "--include-dir", help="include_directory",  type=click.Path(exists=True), multiple=True)
def codegen(config_file, output_dir, include_dir):
    """generate C++ code for a new grunk plugin

    Given an existing C++ library, this command generates a grunk plugin
    that registers all class, struct and function definitions declared
    in header files. These header files are provided in a yaml configuration
    file, refer to the documentation for the format of this file.

    You can structure the registered classes into a hierarchy of submodules
    """
    return generate(config_file, output_dir, list(include_dir))

