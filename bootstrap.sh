#!/bin/sh

set -x
aclocal && \
automake --add-missing --copy --foreign && \
autoconf

