"""Console script for grunk."""

import os
import sys

import click

import grunk
import grunk.plugin_manager as pm
from grunk._util import deconstruct_package_string
from grunk.codegen import generate
from conans.errors import ConanException


@click.group()
@click.version_option(grunk.__version__)
def cli():
    """
    command line interface fro grunk.
    """
    pass


@cli.command()
@click.argument("packages", nargs=-1)
def install(packages):
    """
    install grunk plugins.

    Accepts a list of package references as an argument. These are of the form
    <package_name> or <package_name>/<package_version> or <package_name>/<package_version>@<user>/<channel>,
    see also the conan documentation.

    The version part can be a string or a version range. If no version is specified grunk installs the latest version.

    You may ommit user/channel for packages in the grunk_center.

    Example:

       grunk install PluginA reflect/[>0.0.1] myplugin/2.0@ford_prefect/release
    """
    for package_str in packages:
        package, version, user, channel = deconstruct_package_string(package_str)
        try:
            pm.install(
                package_name=package,
                package_version=version,
                user=user,
                channel=channel,
            )
        except ConanException:
            print(
                'Could not find package "' + package_str + '" in remotes.'
            )  # TODO: logging


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
    "-r", "--remote", help="remote server", default="grunk_center", show_default=True
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
@click.option("-o", "--output-dir", help="output directory for generated source files")
@click.option("-c", "--config-file", help="path to the yml configuration file")
@click.option("-i", "--include_dir", help="include_directory", multiple=True)
def codegen(config_file, output_dir, include_dir):
    """generate C++ code for a new grunk plugin

    Given an existing C++ library, this command generates a grunk plugin
    that registers all class, struct and function definitions declared
    in header files. These header files are provided in a yaml configuration
    file, refer to the documentation for the format of this file.

    You can structure the registered classes into a hierarchy of submodules
    """
    return generate(config_file, output_dir, list(include_dir))
