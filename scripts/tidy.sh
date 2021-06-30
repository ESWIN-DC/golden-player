#!/bin/bash

find include  src samples -name \*.cpp  -exec clang-format -i {} \;
find include  src samples -name \*.h  -exec clang-format -i {} \;

