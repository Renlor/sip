# Copyright (c) 2019, Riverbank Computing Limited
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


import os
import shutil
import sys

from .configuration import Configurable
from .exceptions import BuilderException


class Package:
    """ Encapsulate a package containing one or more extension modules. """

    def __init__(self, name, *, version='1.0', enable_configuration_file=True, sip_module=None, installed_sip_h_dir=None, context_factory=None, bindings_factory=None, builder_factory=None):
        """ Initialise the object. """

        # Provide defaults for remaining unspecified arguments.
        if sip_module is None:
            sip_module = name + '.sip'

        if context_factory is None:
            from .context import ConfigurableContext as context_factory

        if bindings_factory is None:
            from .bindings import ConfigurableBindings as bindings_factory

        if builder_factory is None:
            from .builder import DistutilsBuilder as builder_factory

        self._name = name
        self._version = version
        self.sip_module = sip_module
        self.installed_sip_h_dir = installed_sip_h_dir
        self._context = context_factory()
        self._bindings_factory = bindings_factory
        self._builder_factory = builder_factory

        self._bindings = []

        # Get the configuration.
        try:
            self._configuration = self._configure(enable_configuration_file)
        except Exception as e:
            self._handle_exception(e)

        # Configure the context.
        if isinstance(self._context, Configurable):
            self._context.configure(self._configuration)

        # The build directory is relative to the current directory but all
        # file and directory names are relative to the directory containing
        # this script.
        self.build_dir = os.path.abspath(self._context.build_dir)
        os.chdir(os.path.dirname(__file__))

    def add_bindings(self, sip_file):
        """ Add the bindings defined by a .sip file to the package. """

        self._bindings.append(self.bindings_factory(sip_file))

    def build(self):
        """ Build the package. """

        try:
            if self._context.action == 'install':
                self._install()
            elif self._context.action == 'sdist':
                self._create_sdist()
            elif self._context.action == 'wheel':
                self._create_wheel()

        except Exception as e:
            self._handle_exception(e)

    def information(self, message):
        """ Print an informational message if verbose messages are enabled. """

        if self._context.verbose:
            print(message)

    def progress(self, message):
        """ Print a progress message is verbose messages are enabled. """

        self.information(message + '...')

    def _configure(self, enable_configuration_file):
        """ Return a mapping of user supplied configuration names and values.
        """

        parser = ConfigurationParser(self.version, enable_configuration_file)

        if isinstance(self._context, Configurable):
            parse.add_options(self.context)

        if issubclass(self._bindings_factory, Configurable):
            parse.add_options(self._bindings_factory)

        if issubclass(self._builder_factory, Configurable):
            parse.add_options(self._builder_factory)

        # Parse the configuration.
        return parser.parse()

    def _create_sdist(self):
        """ Create an sdist for the package. """

        raise NotImplemented

    def _create_wheel(self):
        """ Create a wheel for the package. """

        self._set_up_build_dir()

        raise NotImplemented

    def _handle_exception(self, e):
        """ Handle an exception. """

        script_name = os.path.basename(sys.argv[0])

        if isinstance(BuilderException):
            # An "expected" exception.
            if e.detail != '':
                message = "{0}: {1}".format(e.text, e.detail)
            else:
                message = e.text

            print("{0}: {1}".format(script_name, message), file=sys.stderr)

            sys.exit(1)

        # An internal error.
        print("{0}: An internal error occurred...".format(script_name),
                file=sys.stderr)

        raise e

    def _install(self):
        """ Install the package. """

        self._set_up_build_dir()

        raise NotImplemented

    def _set_up_build_dir(self):
        """ Set up the build directory. """

        # Make sure we have a clean build directory.
        shutil.rmtree(self.build_dir, ignore_errors=True)
        os.mkdir(self.build_dir)

        # Create sip.h if we haven't been given a pre-installed copy.
        if self.installed_sip_h_dir is None:
            from ..module.module import module

            module(self._sip_module, include_dir=self._build_dir)
            self.installed_sip_h_dir = self._build_dir

        # Generate the source code for each module's bindings.
        for bindings in self._bindings:
            locations = bindings.generate(self)