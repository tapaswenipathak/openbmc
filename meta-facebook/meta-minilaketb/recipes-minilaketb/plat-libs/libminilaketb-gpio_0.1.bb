# Copyright 2015-present Facebook. All Rights Reserved.
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
# You should have received a copy of the GNU General Public License
# along with this program in a file named COPYING; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301 USA

SUMMARY = "MINILAKETB GPIO Pin Library"
DESCRIPTION = "library for all gpio pins in minilaketb"
SECTION = "base"
PR = "r1"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://minilaketb_gpio.c;beginline=6;endline=18;md5=da35978751a9d71b73679307c4d296ec"


SRC_URI = "file://minilaketb_gpio \
          "

DEPENDS += "libbic "

S = "${WORKDIR}/minilaketb_gpio"

do_install() {
	  install -d ${D}${libdir}
    install -m 0644 libminilaketb_gpio.so ${D}${libdir}/libminilaketb_gpio.so

    install -d ${D}${includedir}
    install -d ${D}${includedir}/facebook
    install -m 0644 minilaketb_gpio.h ${D}${includedir}/facebook/minilaketb_gpio.h
}

FILES:${PN} = "${libdir}/libminilaketb_gpio.so"
FILES:${PN}-dev = "${includedir}/facebook/minilaketb_gpio.h"
