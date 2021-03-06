/*
 * Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
 * Copyright © 2018 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#if !defined(_BTE_SEQ) || !defined(_BTE_NOQ)
#error "Must define _BTE_SEQ and _BTE_NOQ before including this file"
#endif

_BTE_NOQ(NUL, CONTROL, 0x00, NONE, 0, NONE)
_BTE_NOQ(SOH, CONTROL, 0x01, NONE, 0, NONE)
_BTE_NOQ(STX, CONTROL, 0x02, NONE, 0, NONE)
_BTE_NOQ(ETX, CONTROL, 0x03, NONE, 0, NONE)
_BTE_NOQ(EOT, CONTROL, 0x04, NONE, 0, NONE)
_BTE_NOQ(ENQ, CONTROL, 0x05, NONE, 0, NONE)
_BTE_NOQ(ACK, CONTROL, 0x06, NONE, 0, NONE)
_BTE_SEQ(BEL, CONTROL, 0x07, NONE, 0, NONE)
_BTE_SEQ(BS,  CONTROL, 0x08, NONE, 0, NONE)
_BTE_SEQ(HT,  CONTROL, 0x09, NONE, 0, NONE)
_BTE_SEQ(LF,  CONTROL, 0x0a, NONE, 0, NONE)
_BTE_SEQ(VT,  CONTROL, 0x0b, NONE, 0, NONE)
_BTE_SEQ(FF,  CONTROL, 0x0c, NONE, 0, NONE)
_BTE_SEQ(CR,  CONTROL, 0x0d, NONE, 0, NONE)
_BTE_SEQ(LS1, CONTROL, 0x0e, NONE, 0, NONE)
_BTE_SEQ(LS0, CONTROL, 0x0f, NONE, 0, NONE)
_BTE_NOQ(DLE, CONTROL, 0x10, NONE, 0, NONE)
_BTE_NOQ(DC1, CONTROL, 0x11, NONE, 0, NONE)
_BTE_NOQ(DC2, CONTROL, 0x12, NONE, 0, NONE)
_BTE_NOQ(DC3, CONTROL, 0x13, NONE, 0, NONE)
_BTE_NOQ(DC4, CONTROL, 0x14, NONE, 0, NONE)
_BTE_NOQ(SYN, CONTROL, 0x16, NONE, 0, NONE)
_BTE_NOQ(ETB, CONTROL, 0x17, NONE, 0, NONE)
_BTE_NOQ(EM,  CONTROL, 0x19, NONE, 0, NONE)
_BTE_SEQ(SUB, CONTROL, 0x1a, NONE, 0, NONE)
_BTE_NOQ(IS4, CONTROL, 0x1c, NONE, 0, NONE)
_BTE_NOQ(IS3, CONTROL, 0x1d, NONE, 0, NONE)
_BTE_NOQ(IS2, CONTROL, 0x1e, NONE, 0, NONE)
_BTE_NOQ(IS1, CONTROL, 0x1f, NONE, 0, NONE)
_BTE_NOQ(BPH, CONTROL, 0x82, NONE, 0, NONE)
_BTE_NOQ(NBH, CONTROL, 0x83, NONE, 0, NONE)
_BTE_SEQ(IND, CONTROL, 0x84, NONE, 0, NONE)
_BTE_SEQ(NEL, CONTROL, 0x85, NONE, 0, NONE)
_BTE_NOQ(SSA, CONTROL, 0x86, NONE, 0, NONE)
_BTE_NOQ(ESA, CONTROL, 0x87, NONE, 0, NONE)
_BTE_SEQ(HTS, CONTROL, 0x88, NONE, 0, NONE)
_BTE_SEQ(HTJ, CONTROL, 0x89, NONE, 0, NONE)
_BTE_NOQ(VTS, CONTROL, 0x8a, NONE, 0, NONE)
_BTE_NOQ(PLD, CONTROL, 0x8b, NONE, 0, NONE)
_BTE_NOQ(PLU, CONTROL, 0x8c, NONE, 0, NONE)
_BTE_SEQ(RI,  CONTROL, 0x8d, NONE, 0, NONE)
_BTE_SEQ(SS2, CONTROL, 0x8e, NONE, 0, NONE)
_BTE_SEQ(SS3, CONTROL, 0x8f, NONE, 0, NONE)
_BTE_NOQ(PU1, CONTROL, 0x91, NONE, 0, NONE)
_BTE_NOQ(PU2, CONTROL, 0x92, NONE, 0, NONE)
_BTE_NOQ(STS, CONTROL, 0x93, NONE, 0, NONE)
_BTE_NOQ(CCH, CONTROL, 0x94, NONE, 0, NONE)
_BTE_NOQ(NAK, CONTROL, 0x95, NONE, 0, NONE)
_BTE_NOQ(SPA, CONTROL, 0x96, NONE, 0, NONE)
_BTE_NOQ(EPA, CONTROL, 0x97, NONE, 0, NONE)
_BTE_NOQ(ST,  CONTROL, 0x9c, NONE, 0, NONE)
