#!/bin/sh

set -x
libtoolize --copy && \
aclocal && \
automake --add-missing --copy --foreign && \
autoconf

