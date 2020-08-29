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


from typing import overload, Sequence, Union


# Constants.
SIP_VERSION = ...       # type: int
SIP_VERSION_STR = ...   # type: str


# The bases for SIP generated types.
class wrappertype: ...
class simplewrapper: ...
class wrapper(simplewrapper): ...


# PEP 484 has no explicit support for the buffer protocol so we just name types
# we know that implement it.
Buffer = Union['array', 'voidptr', str, bytes, bytearray]


# The array type.
class array(Sequence): ...


# The voidptr type.
class voidptr:

    def __init__(addr: Union[int, Buffer], size: int = -1, writeable: bool = True) -> None: ...

    def __int__(self) -> int: ...

    @overload
    def __getitem__(self, i: int) -> bytes: ...

    @overload
    def __getitem__(self, s: slice) -> 'voidptr': ...

    def __hex__(self) -> str: ...

    def __len__(self) -> int: ...

    def __setitem__(self, i: Union[int, slice], v: Buffer) -> None: ...

    def asarray(self, size: int = -1) -> array: ...

    # Python doesn't expose the capsule type.
    #def ascapsule(self) -> capsule: ...

    def asstring(self, size: int = -1) -> bytes: ...

    def getsize(self) -> int: ...

    def getwriteable(self) -> bool: ...

    def setsize(self, size: int) -> None: ...

    def setwriteable(self, bool) -> None: ...


# Remaining functions.
def assign(obj: simplewrapper, other: simplewrapper) -> None: ...
def cast(obj: simplewrapper, type: wrappertype) -> simplewrapper: ...
def delete(obj: simplewrapper) -> None: ...
def dump(obj: simplewrapper) -> None: ...
def enableautoconversion(type: wrappertype, enable: bool) -> bool: ...
def enableoverflowchecking(enable: bool) -> bool: ...
def getapi(name: str) -> int: ...
def isdeleted(obj: simplewrapper) -> bool: ...
def ispycreated(obj: simplewrapper) -> bool: ...
def ispyowned(obj: simplewrapper) -> bool: ...
def setapi(name: str, version: int) -> None: ...
def setdeleted(obj: simplewrapper) -> None: ...
def setdestroyonexit(destroy: bool) -> None: ...
def settracemask(mask: int) -> None: ...
def transferback(obj: wrapper) -> None: ...
def transferto(obj: wrapper, owner: wrapper) -> None: ...
def unwrapinstance(obj: simplewrapper) -> None: ...
def wrapinstance(addr: int, type: wrappertype) -> simplewrapper: ...
