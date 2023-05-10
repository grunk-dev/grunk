#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""Tests for `grunk` command line interface."""

from click.testing import CliRunner

from grunk import cli, __version__


def test_cli():
    """Test the CLI."""
    runner = CliRunner()
    help_result = runner.invoke(cli.cli, ["--help"])
    assert "grunk" in help_result.output
    assert help_result.exit_code == 0
    assert "--help     Show this message and exit." in help_result.output
    help_result = runner.invoke(cli.cli, ["--version"])
    assert help_result.exit_code == 0
    assert __version__ in help_result.output


def test_cli_install():
    runner = CliRunner()
    result = runner.invoke(cli.cli, ["install", "nonexistent"])
    assert result.exit_code == 0
    assert 'Could not find package "nonexistent" in remotes.\n' in result.output
