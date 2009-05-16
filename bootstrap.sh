#!/bin/sh

set -x
libtoolize --copy --force && \
aclocal && \
automake --add-missing --copy --foreign && \
autoconf

