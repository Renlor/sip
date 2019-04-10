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


from argparse import ArgumentParser

from .version import SIP_VERSION_STR


class StandardArgumentParser(ArgumentParser):
    """ A sub-class of argparse.ArgumentParser that implements standard
    arguments.
    """

    def __init__(self, **kwargs):
        """ Initialise the object. """

        super().__init__(**kwargs)

        self.add_argument('-V', '--version', action='version',
                version=SIP_VERSION_STR)

    def add_include_dir_option(self):
        """ Add the standard option for the include directory. """

        self.add_argument('-I', '--include-dir', dest='include_dirs',
                action='append',
                help="add <DIR> to the list of directories to search when "
                        "importing or including .sip files",
                metavar="DIR")

    def add_warnings_options(self):
        """ Add the standard options related to warnings. """

        self.add_argument('-w', dest='warnings', action='store_true',
                default=False,
                help="enable warning messages [default disabled]")

        self..add_argument('-f', '--warnings-are-errors',
                dest='warnings_are_errors', action='store_true', default=False,
                help="warnings are handled as errors")
