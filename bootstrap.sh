#!/bin/sh

set -x
libtoolize --copy --force && \
aclocal && \
autoheader && \
automake --add-missing --copy --foreign && \
autoconf

