# Copyright 2019-present Facebook. All Rights Reserved.
#
# This program file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#

SUMMARY = "I2C module drivers"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://COPYING;md5=12f884d2ae1ff87c09e5b7ccc2c4ca7e"

inherit module kernel_extra_headers_export

PR = "r0"
PV = "0.1"

SRC_URI = "file://Makefile \
           file://ltc4282.c \
           file://max6615.c \
           file://COPYING \
          "

S = "${WORKDIR}"

DEPENDS += "kernel-module-i2c-dev-sysfs"

RDEPENDS:${PN} += "kernel-module-i2c-dev-sysfs"

KERNEL_MODULE_AUTOLOAD += ""
