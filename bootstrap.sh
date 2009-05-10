#!/bin/sh

set -x
mkdir -p config && \
aclocal -I config && \
automake --add-missing --copy --foreign && \
autoconf

