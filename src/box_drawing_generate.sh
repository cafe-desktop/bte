#!/usr/bin/env bash
# Copyright © 2014 Egmont Koblinger
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

cat <<"END"
/* Generated by box_drawing_generate.sh; do not edit! */

#define BTE_BOX_DRAWING_5_BY_5_BITMAP(x11, x12, x13, x14, x15, \
                                      x21, x22, x23, x24, x25, \
                                      x31, x32, x33, x34, x35, \
                                      x41, x42, x43, x44, x45, \
                                      x51, x52, x53, x54, x55) \
	(((x11) << 24) | ((x12) << 23) | ((x13) << 22) | ((x14) << 21) | ((x15) << 20) | \
	 ((x21) << 19) | ((x22) << 18) | ((x23) << 17) | ((x24) << 16) | ((x25) << 15) | \
	 ((x31) << 14) | ((x32) << 13) | ((x33) << 12) | ((x34) << 11) | ((x35) << 10) | \
	 ((x41) <<  9) | ((x42) <<  8) | ((x43) <<  7) | ((x44) <<  6) | ((x45) <<  5) | \
	 ((x51) <<  4) | ((x52) <<  3) | ((x53) <<  2) | ((x54) <<  1) | ((x55)))

/* Definition of most of the glyphs in the 2500..257F range as 5x5 bitmaps
   (bits 24..0 in the obvious order), see bug 709556 and ../doc/boxes.txt */
static const guint32 _bte_draw_box_drawing_bitmaps[128] = {
END

LC_ALL=C
pos=$((0x2500))
while [ $pos -lt $((0x2580)) ]; do
	read header
	echo -e "\\t/* $header */"
	echo -e "\\tBTE_BOX_DRAWING_5_BY_5_BITMAP("
	for y in 1 2 3 4 5; do
		echo -ne '\t\t'
		read line
		for x in 1 2 3 4 5; do
			if [ "${line:0:3}" == "▓" ]; then
				echo -n '1'
			else
				echo -n '0'
			fi
			line="${line:3}"
			if [ $x = 5 -a $y = 5 ]; then
				echo -n ')'
			fi
			echo -n ', '
			if [ $x = 5 ]; then
				echo
			fi
		done
	done
	echo
	pos=$((pos+1))
done < "$1"

cat <<"END"
};

#undef BTE_BOX_DRAWING_5_BY_5_BITMAP
END
