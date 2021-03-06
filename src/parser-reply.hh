/*
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

#if !defined(_BTE_REPLY)
#error "Must define _BTE_REPLY before including this file"
#endif

_BTE_REPLY(NONE, NONE, 0, NONE, NONE,) /* placeholder */


_BTE_REPLY(APC,         APC, 0,   NONE, NONE,  ) /* application program command */
_BTE_REPLY(DECEKBD,     APC, 0,   NONE, NONE,  ) /* extended keyboard report */

_BTE_REPLY(XTERM_FOCUS_IN,                            CSI, 'I', NONE, NONE,  ) /* XTERM focus in report */
_BTE_REPLY(XTERM_MOUSE_EXT_SGR_REPORT_BUTTON_PRESS,   CSI, 'M', LT,   NONE,  ) /* XTERM SGR mouse mode button press report */
_BTE_REPLY(XTERM_FOCUS_OUT,                           CSI, 'O', NONE, NONE,  ) /* XTERM focus out report */
_BTE_REPLY(DECXCPR,                                   CSI, 'R', WHAT, NONE,  ) /* extended cursor position report */
_BTE_REPLY(CPR,                                       CSI, 'R', NONE, NONE,  ) /* cursor position report */
_BTE_REPLY(DECDA1R,                                   CSI, 'c', WHAT, NONE,  ) /* DA1 report */
_BTE_REPLY(DECDA2R,                                   CSI, 'c', GT,   NONE,  ) /* DA2 report */
_BTE_REPLY(SGR,                                       CSI, 'm', NONE, NONE,  ) /* SGR */
_BTE_REPLY(XTERM_MOUSE_EXT_SGR_REPORT_BUTTON_RELEASE, CSI, 'm', LT,   NONE,  ) /* XTERM SGR mouse mode button release report */
_BTE_REPLY(DSR,                                       CSI, 'n', NONE, NONE,  ) /* device status report */
_BTE_REPLY(DECDSR,                                    CSI, 'n', WHAT, NONE,  ) /* device status report */
_BTE_REPLY(DECSCUSR,                                  CSI, 'q', NONE, SPACE, ) /* set-cursor-style */
_BTE_REPLY(DECSRC,                                    CSI, 'q', NONE, MULT,  ) /* secure reset confirmation */
_BTE_REPLY(DECSTBM,                                   CSI, 'r', NONE, NONE,  ) /* set-top-and-bottom-margins */
_BTE_REPLY(XTERM_WM,                                  CSI, 't', NONE, NONE,  ) /* XTERM WM report */
_BTE_REPLY(DECRPDE,                                   CSI, 'w', NONE, DQUOTE,) /* report displayed extent */
_BTE_REPLY(DECRPKT,                                   CSI, 'v', NONE, COMMA, ) /* report key type */
_BTE_REPLY(DECREPTPARM,                               CSI, 'x', NONE, NONE,  ) /* report terminal parameters */
_BTE_REPLY(DECPKMFR,                                  CSI, 'y', NONE, PLUS,  ) /* program key free memory report */
_BTE_REPLY(DECRPM_ECMA,                               CSI, 'y', NONE, CASH,  ) /* report ECMA mode */
_BTE_REPLY(DECRPM_DEC,                                CSI, 'y', WHAT, CASH,  ) /* report private mode */
_BTE_REPLY(DECMSR,                                    CSI, '{', NONE, MULT,  ) /* macro space report */
_BTE_REPLY(DECFNK,                                    CSI, '~', NONE, NONE,  ) /* dec function key / XTERM bracketed paste */

_BTE_REPLY(DECTABSR,    DCS, '@', NONE, CASH,  ) /* tabulation stop report */
_BTE_REPLY(DECRPSS,     DCS, 'r', NONE, CASH,  ) /* report state or setting */
_BTE_REPLY(DECTSR,      DCS, 's', NONE, CASH,  ) /* terminal state report */
_BTE_REPLY(DECCTR,      DCS, 's', NONE, CASH,  _BTE_REPLY_PARAMS({2})) /* color table report */
_BTE_REPLY(DECAUPSS,    DCS, 'u', NONE, BANG,  ) /* assign user preferred supplemental set */
_BTE_REPLY(DECCIR,      DCS, 'u', NONE, CASH,  _BTE_REPLY_PARAMS({1})) /* cursor information report */
_BTE_REPLY(DECRPTUI,    DCS, '|', NONE, BANG,  _BTE_REPLY_STRING("7E565445") /* "~BTE" */) /* report terminal unit ID */
_BTE_REPLY(DECRPFK,     DCS, '}', NONE, DQUOTE,) /* report function key */
_BTE_REPLY(DECCKSR,     DCS, '~', NONE, BANG,  ) /* memory checksum report */
_BTE_REPLY(DECRPAK,     DCS, '~', NONE, DQUOTE,) /* report all modifiers/alphanumeric key */

_BTE_REPLY(OSC,         OSC, 0,   NONE, NONE,  ) /* operating system command */

_BTE_REPLY(PM,          PM,  0,   NONE, NONE,  ) /* privacy message */

_BTE_REPLY(SOS,         SOS, 0,   NONE, NONE,  ) /* start of string */
