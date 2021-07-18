#!/bin/bash

SCRIPT_HOME="$(dirname "$( readlink -m $( type -p ${0} ))")"

cd "${SCRIPT_HOME}/.."
find src include examples  -type f \( -name \*.h -or -name \*.cpp \)  -exec clang-format -i {} \;
