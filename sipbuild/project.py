# Copyright (c) 2020, Riverbank Computing Limited
# All rights reserved.
#
# This copy of SIP is licensed for use under the terms of the SIP License
# Agreement.  See the file LICENSE for more details.
#
# This copy of SIP may also used under the terms of the GNU General Public
# License v2 or v3 as published by the Free Software Foundation which can be
# found in the files LICENSE-GPL2 and LICENSE-GPL3 included in this package.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.


from collections import OrderedDict
from distutils.sysconfig import get_python_inc, get_python_lib
import os
import packaging
import shutil
import subprocess
import sys
import tempfile
import warnings

from .abstract_builder import AbstractBuilder
from .abstract_project import AbstractProject
from .bindings import Bindings
from .configurable import Configurable, Option
from .exceptions import UserException
from .module import resolve_abi_version
from .py_versions import FIRST_SUPPORTED_MINOR, LAST_SUPPORTED_MINOR
from .pyproject import (PyProjectException, PyProjectOptionException,
        PyProjectUndefinedOptionException)


class Project(AbstractProject, Configurable):
    """ Encapsulate a project containing one or more sets of bindings. """

    # The configurable options.
    _options = (
        # The ABI version that the sip module should use.
        Option('abi_version'),

        # The callable that will return an Bindings instance.  This is used for
        # bindings implicitly defined in the .toml file.
        Option('bindings_factory', markers=False),

        # The callable that will return an AbstractBuilder instance.
        Option('builder_factory', markers=False),

        # The list of console script entry points.
        Option('console_scripts', option_type=list),

        # Set if an __init__.py should be installed.
        Option('dunder_init', option_type=bool, default=False),

        # The minimum GLIBC version required by the project.  This is used to
        # determine the correct platform tag to use for Linux wheels.
        Option('minimum_glibc_version'),

        # Set if building for a debug version of Python.
        Option('py_debug', option_type=bool),

        # The name of the directory containing Python.h.
        Option('py_include_dir', default=get_python_inc()),

        # The name of the target Python platform.
        Option('py_platform'),

        # The major version number of the target Python installation.
        Option('py_major_version', option_type=int),

        # The minor version number of the target Python installation.
        Option('py_minor_version', option_type=int),

        # The name of the directory containing the .sip files.  If the sip
        # module is shared then each set of bindings is in its own
        # sub-directory.
        Option('sip_files_dir', default=os.getcwd()),

        # The list of files and directories, specified as glob patterns
        # relative to the project directory, that should be excluded from an
        # sdist.
        Option('sdist_excludes', option_type=list),

        # The list of additional directories to search for .sip files.
        Option('sip_include_dirs', option_type=list),

        # The fully qualified name of the sip module.
        Option('sip_module'),

        # The user-configurable options.
        Option('quiet', option_type=bool,
                help="disable all progress messages"),
        Option('verbose', option_type=bool,
                help="enable verbose progress messages"),
        Option('name', help="the name used in sdist and wheel file names",
                metavar="NAME", tools=['sdist', 'wheel']),
        Option('build_dir', help="the build directory", metavar="DIR"),
        Option('build_tag', help="the build tag to be used in the wheel name",
                metavar="TAG", tools=['wheel']),
        Option('target_dir', default=get_python_lib(plat_specific=1),
                help="the target installation directory", metavar="DIR",
                tools=['build', 'install']),
        Option('api_dir', help="generate a QScintilla .api file in DIR",
                metavar="DIR"),
    )

    # The configurable options for multiple bindings.
    _multibindings_options = (
        Option('disable', option_type=list, help="disable the NAME bindings",
                metavar="NAME"),
        Option('enable', option_type=list, help="enable the NAME bindings",
                metavar="NAME"),
    )

    def __init__(self, **kwargs):
        """ Initialise the project. """

        super().__init__(**kwargs)

        # The current directory should contain the .toml file.
        self.root_dir = os.getcwd()
        self.bindings = OrderedDict()
        self.bindings_factories = []
        self.builder = None
        self.buildables = []
        self.installables = []

        self._temp_build_dir = None

    def apply_nonuser_defaults(self, tool):
        """ Set default values for non-user options that haven't been set yet.
        """

        if self.bindings_factory is None:
            self.bindings_factory = Bindings
        elif isinstance(self.bindings_factory, str):
            # Convert the name to a callable.
            self.bindings_factory = self.import_callable(self.bindings_factory,
                    Bindings)

        if self.builder_factory is None:
            from .distutils_builder import DistutilsBuilder
            self.builder_factory = DistutilsBuilder
        elif isinstance(self.builder_factory, str):
            # Convert the name to a callable.
            self.builder_factory = self.import_callable(self.builder_factory,
                    AbstractBuilder)

        if self.py_major_version is None or self.py_minor_version is None:
            self.py_major_version = sys.hexversion >> 24
            self.py_minor_version = (sys.hexversion >> 16) & 0x0ff

        if self.py_platform is None:
            self.py_platform = sys.platform

        if self.py_debug is None:
            self.py_debug = hasattr(sys, 'gettotalrefcount')

        super().apply_nonuser_defaults(tool)

    def apply_user_defaults(self, tool):
        """ Set default values for user options that haven't been set yet. """

        # If we the backend to a 3rd-party frontend (most probably pip) then
        # let it handle the verbosity of messages.
        if self.verbose is None and tool == '':
            self.verbose = True

        # This is only used when creating sdist and wheel files.
        if self.name is None:
            self.name = self.metadata['name']

        # For the build tool we want build_dir to default to a local 'build'
        # directory (which we won't remove).  However, for other tools (and for
        # PEP 517 frontends) we want to use a temporary directory in case the
        # current directory is read-only.
        if self.build_dir is None:
            if tool == 'build':
                self.build_dir = 'build'
            else:
                self._temp_build_dir = tempfile.TemporaryDirectory()
                self.build_dir = self._temp_build_dir.name

        super().apply_user_defaults(tool)

        # Adjust the list of bindings according to what has been explicitly
        # enabled and disabled.
        self._enable_disable_bindings()

        # Set the user defaults for the builder and bindings.
        self.builder.apply_user_defaults(tool)

        for bindings in self.bindings.values():
            bindings.apply_user_defaults(tool)

    def build(self):
        """ Build the project in-situ. """

        self.builder.build()

    def build_sdist(self, sdist_directory):
        """ Build an sdist for the project and return the name of the sdist
        file.
        """

        sdist_file = self.builder.build_sdist(sdist_directory)
        self._remove_build_dir()

        return sdist_file

    def build_wheel(self, wheel_directory):
        """ Build a wheel for the project and return the name of the wheel
        file.
        """

        wheel_file = self.builder.build_wheel(wheel_directory)
        self._remove_build_dir()

        return wheel_file

    def get_bindings_dir(self):
        """ Return the name of the 'bindings' directory relative to the
        eventual target directory.
        """

        name_parts = self.sip_module.split('.')
        name_parts[-1] = 'bindings'

        return os.path.join(*name_parts)

    def get_distinfo_dir(self, target_dir):
        """ Return the name of the .dist-info directory for a target directory.
        """

        return os.path.join(target_dir,
                '{}-{}.dist-info'.format(self.name.replace('-', '_'),
                self.version_str))

    def get_dunder_init(self):
        """ Return the contents of the __init__.py to install. """

        # This default implementation will create an empty file.
        return ''

    def get_options(self):
        """ Return the list of configurable options. """

        options = super().get_options()
        options.extend(self._options)
        options.extend(self._multibindings_options)

        return options

    def get_requires_dists(self):
        """ Return any 'Requires-Dist' to add to the project's meta-data. """

        # The only requirement is for the sip module.
        if not self.sip_module:
            return []

        requires_dist = self.metadata.get('requires-dist')
        if requires_dist is None:
            requires_dist = []
        elif isinstance(requires_dist, str):
            requires_dist = [requires_dist]

        # Ignore if the module is already defined.
        sip_project_name = self.sip_module.replace('.', '-')

        for rd in requires_dist:
            if rd.split()[0] == sip_project_name:
                return []

        next_abi_major = int(self.abi_version.split('.')[0]) + 1

        return ['{} (>={}, <{})'.format(sip_project_name, self.abi_version,
                next_abi_major)]

    def install(self):
        """ Install the project. """

        self.builder.install()
        self._remove_build_dir()

    @staticmethod
    def open_for_writing(fname):
        """ Open a file for writing while handling any errors. """

        try:
            return open(fname, 'w')
        except IOError as e:
            raise UserException(
                    "There was an error creating '{0}' - make sure you have "
                    " write permission on the parent directory".format(fname),
                    detail=str(e))

    def progress(self, message):
        """ Print a progress message unless they are disabled. """

        if not self.quiet:
            if message[-1] != '.':
                message += '...'

            print(message, flush=True)

    def read_command_pipe(self, args, *, and_stderr=False, fatal=True):
        """ A generator for each line of a pipe from a command's stdout. """

        cmd = ' '.join(args)

        if self.verbose:
            print(cmd, flush=True)

        stderr = subprocess.STDOUT if and_stderr else subprocess.PIPE

        pipe = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE,
                stdout=subprocess.PIPE, stderr=stderr)

        for line in pipe.stdout:
            yield str(line, encoding=sys.stdout.encoding)

        rc = pipe.wait()
        if rc != 0 and fatal:
            raise UserException("'{0}' failed returning {1}".format(cmd, rc))

    def run_command(self, args, *, fatal=True):
        """ Run a command and display the output if requested. """

        # Read stdout and stderr until there is no more output.
        for line in self.read_command_pipe(args, and_stderr=True, fatal=fatal):
            if self.verbose:
                sys.stdout.write(line)

    def setup(self, pyproject, tool, tool_description):
        """ Complete the configuration of the project. """

        # Create any programmatically defined bindings.
        for bindings_factory in self.bindings_factories:
            bindings = bindings_factory(self)
            self.bindings[bindings.name] = bindings

        # Set the initial configuration from the pyproject.toml file.
        self._set_initial_configuration(pyproject, tool)

        # Add any tool-specific command line options for (so far unspecified)
        # parts of the configuration.
        if tool != 'pep517':
            self._configure_from_command_line(tool, tool_description)
        else:
            # Until pip improves it's error reporting we give the user all the
            # help we can.
            self.verbose = True

        # Now that any help has been given we can report a missing
        # pyproject.toml file.
        if pyproject.pyproject_missing:
            raise PyProjectException(
                    "there is no such file in the current directory")

        # Make sure the configuration is complete.
        self.apply_user_defaults(tool)

        # Configure the warnings module.
        if not self.verbose:
            warnings.simplefilter('ignore', UserWarning)

        # Make sure we have a clean build directory and make it current.
        if self._temp_build_dir is None:
            self.build_dir = os.path.abspath(self.build_dir)
            shutil.rmtree(self.build_dir, ignore_errors=True)
            os.mkdir(self.build_dir)

        os.chdir(self.build_dir)

        # Allow a sub-class (in a user supplied script) to make any updates to
        # the configuration.
        self.update(tool)

        os.chdir(self.root_dir)

        # Make sure the configuration is correct after any user supplied script
        # has messed with it.
        self.verify_configuration(tool)

        if tool in Option.BUILD_TOOLS and self.bindings:
            self.progress(
                    "These bindings will be built: {}.".format(
                            ', '.join(self.bindings.keys())))

    def update(self, tool):
        """ This should be re-implemented by any user supplied sub-class to
        carry out any updates to the configuration as required.  The current
        directory will be the temporary build directory.
        """

        # This default implementation calls update_buildable_bindings().
        if tool in Option.BUILD_TOOLS:
            self.update_buildable_bindings()

    def update_buildable_bindings(self):
        """ Update the list of bindings to ensure they are either buildable or
        have been explicitly enabled.
        """

        # Explicitly enabled bindings are assumed to be buildable.
        if self.enable:
            return

        for b in list(self.bindings.values()):
            if not b.is_buildable():
                del self.bindings[b.name]

    def verify_configuration(self, tool):
        """ Verify that the configuration is complete and consistent. """

        # Make sure any build tag is valid.
        if self.build_tag is not None:
            if self.build_tag == '' or not self.build_tag[0].isdigit():
                raise PyProjectOptionException('build-tag',
                        "must begin with a digit", section='tool.sip.project')

        # Make sure any minimum GLIBC version is valid and convert it to a
        # 2-tuple.
        if self.minimum_glibc_version is None:
            self.minimum_glibc_version = (2, 5)
        else:
            parts = self.minimum_glibc_version.split('.')

            try:
                if len(parts) != 2:
                    raise ValueError()

                self.minimum_glibc_version = (int(parts[0]), int(parts[1]))
            except ValueError:
                raise PyProjectOptionException('minimum-glibc-version',
                        "'{0}' is an invalid GLIBC version number".format(
                                self.minimum_glibc_version),
                        section_name='tool.sip.project')

        # Make sure relevent paths are absolute and use native separators.
        self.sip_files_dir = self.sip_files_dir.replace('/', os.sep)
        if not os.path.isabs(self.sip_files_dir):
            self.sip_files_dir = os.path.join(self.root_dir,
                    self.sip_files_dir)

        # Make sure we support the targeted version of Python.
        py_version = (self.py_major_version, self.py_minor_version)
        first_version = (3, FIRST_SUPPORTED_MINOR)
        last_version = (3, LAST_SUPPORTED_MINOR)

        if py_version < first_version or py_version > last_version:
            raise UserException(
                    "Python v{}.{} is not supported".format(
                            self.py_major_version, self.py_minor_version))

        # Make sure we have a valid ABI version.
        self.abi_version = resolve_abi_version(self.abi_version)

        # Checks for standalone projects.
        if not self.sip_module:
            # Check there is only one set of bindings.
            if len(self.bindings) > 1:
                raise PyProjectOptionException('sip-module',
                        "must be defined when the project contains multiple "
                        "sets of bindings")

            # Make sure __init__.py is disabled.
            self.dunder_init = False

        # Verify the configuration of the builder and bindings.
        self.builder.verify_configuration(tool)

        for bindings in self.bindings.values():
            bindings.verify_configuration(tool)

    def _configure_from_command_line(self, tool, tool_description):
        """ Update the configuration from the user supplied command line. """

        from argparse import SUPPRESS
        from .argument_parser import ArgumentParser

        parser = ArgumentParser(tool_description, argument_default=SUPPRESS)

        # Add the user configurable options to the parser.
        all_options = {}
        
        options = self.get_options()
        if len(self.bindings) < 2:
            # Remove the options that only make sense where the project has
            # multiple bindings.
            for multi in self._multibindings_options:
                options.remove(multi)

        self.add_command_line_options(parser, tool, all_options,
                options=options)

        self.builder.add_command_line_options(parser, tool, all_options)

        for bindings in self.bindings.values():
            bindings.add_command_line_options(parser, tool, all_options)

        # Parse the arguments and update the corresponding configurables.
        args = parser.parse_args()

        for option, configurables in all_options.items():
            for configurable in configurables:
                if hasattr(args, option.dest):
                    setattr(configurable, option.name,
                            getattr(args, option.dest))

    def _enable_disable_bindings(self):
        """ Check the enabled bindings are valid and remove any disabled ones.
        """

        names = list(self.bindings.keys())

        # Check that any explicitly enabled bindings are valid.
        if self.enable:
            for enabled in self.enable:
                if enabled not in names:
                    raise UserException(
                            "unknown enabled bindings '{0}'".format(enabled))

            # Only include explicitly enabled bindings.
            for b in list(self.bindings.values()):
                if b.name not in self.enable:
                    del self.bindings[b.name]

        # Check that any explicitly disabled bindings are valid.
        if self.disable:
            for disabled in self.disable:
                if disabled not in names:
                    raise UserException(
                            "unknown disabled bindings '{0}'".format(disabled))

            # Remove any explicitly disabled bindings.
            for b in list(self.bindings.values()):
                if b.name in self.disable:
                    del self.bindings[b.name]

    def _remove_build_dir(self):
        """ Remove the build directory. """

        self._temp_build_dir = None

    def _set_initial_configuration(self, pyproject, tool):
        """ Set the project's initial configuration. """

        # Get the metadata and extract the version.
        self.metadata = pyproject.get_metadata()
        self.version_str = self.metadata['version']

        # Convert the version as a string to number.
        base_version = packaging.version.parse(self.version_str).base_version
        base_version = base_version.split('.')

        while len(base_version) < 3:
            base_version.append('0')

        version = 0
        for part in base_version:
            version <<= 8

            try:
                version += int(part)
            except ValueError:
                raise PyProjectOptionException('version',
                        "'{0}' is an invalid version number".format(
                                self.version_str),
                        section_name='tool.sip.metadata')

        self.version = version

        # Configure the project.
        self.configure(pyproject, 'tool.sip.project', tool)

        # Create and configure the builder.
        self.builder = self.builder_factory(self)
        self.builder.configure(pyproject, 'tool.sip.builder', tool)

        # For each set of bindings configuration make sure a bindings object
        # exists, creating it if necessary.
        bindings_sections = pyproject.get_section('tool.sip.bindings')
        if bindings_sections is not None:
            for name in bindings_sections.keys():
                if name not in self.bindings:
                    bindings = self.bindings_factory(self, name)
                    self.bindings[bindings.name] = bindings

        # Add a default set of bindings if none were defined.
        if not self.bindings:
            bindings = self.bindings_factory(self, self.metadata['name'])
            self.bindings[bindings.name] = bindings

        # Now configure each set of bindings.
        for bindings in self.bindings.values():
            bindings.configure(pyproject, 'tool.sip.bindings.' + bindings.name,
                    tool)
