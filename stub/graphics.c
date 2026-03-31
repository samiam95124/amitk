/*******************************************************************************
*                                                                              *
*                                PETIT-AMI                                     *
*                                                                              *
*                             GRAPHICS MODULE                                  *
*                                                                              *
*                              Created 2020                                    *
*                                                                              *
*                               S. A. FRANCO                                   *
*                                                                              *
* This is a stubbed version. It exists so that Petit-Ami can be compiled and   *
* linked without all of the modules being complete.                            *
*                                                                              *
* All of the calls in this library print an error and exit.                    *
*                                                                              *
* One use of the stubs module is to serve as a prototype for a new module      *
* implementation.                                                              *
*                                                                              *
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* Copyright (C) 2019 - Scott A. Franco                                         *
*                                                                              *
* All rights reserved.                                                         *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions           *
* are met:                                                                     *
*                                                                              *
* 1. Redistributions of source code must retain the above copyright            *
*    notice, this list of conditions and the following disclaimer.             *
* 2. Redistributions in binary form must reproduce the above copyright         *
*    notice, this list of conditions and the following disclaimer in the       *
*    documentation and/or other materials provided with the distribution.      *
* 3. Neither the name of the project nor the names of its contributors         *
*    may be used to endorse or promote products derived from this software     *
*    without specific prior written permission.                                *
*                                                                              *
* THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND      *
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE        *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   *
* ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE     *
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL   *
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS      *
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)        *
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT   *
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    *
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF       *
* SUCH DAMAGE.                                                                 *
*                                                                              *
*******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <graphics.h>

/********************************************************************************

Process library error

Outputs an error message, then halts.

********************************************************************************/

static void error(char *s)
{

    fprintf(stderr, "\nError: Graphics: %s\n", s);

    exit(1);

}

/*******************************************************************************

Scroll screen

Scrolls the ANSI terminal screen by deltas in any given direction. If the scroll
would move all content off the screen, the screen is simply blanked. Otherwise,
we find the section of the screen that would remain after the scroll, determine
its source and destination rectangles, and use a bitblt to move it.
One speedup for the code would be to use non-overlapping fills for the x-y
fill after the bitblt.

In buffered mode, this routine works by scrolling the buffer, then restoring
it to the current window. In non-buffered mode, the scroll is applied directly
to the window.

*******************************************************************************/

void ami_scrollg(FILE* f, int x, int y)

{

    error("ami_scrollg: Is not implemented");

}

void ami_scroll(FILE* f, int x, int y)

{

    error("ami_scroll: Is not implemented");

}

/*******************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

*******************************************************************************/

void ami_cursor(FILE* f, int x, int y)

{

    error("ami_cursor: Is not implemented");

}

/*******************************************************************************

Position cursor graphical

Moves the cursor to the specified x and y location in pixels.

*******************************************************************************/

void ami_cursorg(FILE* f, int x, int y)

{

    error("ami_cursorg: Is not implemented");

}

/*******************************************************************************

Find character baseline

Returns the offset, from the top of the current fonts character bounding box,
to the font baseline. The baseline is the line all characters rest on.

*******************************************************************************/

int ami_baseline(FILE* f)

{

    error("ami_baseline: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Return maximum x dimension

Returns the maximum x dimension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int ami_maxx(FILE* f)

{

    error("ami_maxx: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Return maximum y dimension

Returns the maximum y dimension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int ami_maxy(FILE* f)

{

    error("ami_maxy: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Return maximum x dimension graphical

Returns the maximum x dimension, which is the width of the client surface in
pixels.

*******************************************************************************/

int ami_maxxg(FILE* f)

{

    error("ami_maxxg: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Return maximum y dimension graphical

Returns the maximum y dimension, which is the height of the client surface in
pixels.

*******************************************************************************/

int ami_maxyg(FILE* f)

{

    error("ami_maxyg: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

void ami_home(FILE* f)

{

    error("ami_home: Is not implemented");

}

/*******************************************************************************

Move cursor up internal

Moves the cursor position up one line.

*******************************************************************************/

void ami_up(FILE* f)

{

    error("ami_up: Is not implemented");

}

/*******************************************************************************

Move cursor down internal

Moves the cursor position down one line.

*******************************************************************************/

void ami_down(FILE* f)

{

    error("ami_down: Is not implemented");

}

/*******************************************************************************

Move cursor left internal

Moves the cursor one character left. If the cursor is at the extreme left and
auto mode is on, the cursor will wrap to the right, up one line, otherwise
the cursor will move into negative space, limited only by maxint.

*******************************************************************************/

void ami_left(FILE* f)

{

    error("ami_left: Is not implemented");

}

/*******************************************************************************

Move cursor right

Moves the cursor one character right.

*******************************************************************************/

void ami_right(FILE* f)

{

    error("ami_right: Is not implemented");

}

/*******************************************************************************

Turn on blink attribute

Turns on/off the blink attribute.

Note that the attributes can only be set singly.

Graphical mode does not implement blink mode.

*******************************************************************************/

void ami_blink(FILE* f, int e)

{

    error("ami_blink: Is not implemented");

}

/*******************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute. Reverse is done by swapping the background
and foreground writing colors.

*******************************************************************************/

void ami_reverse(FILE* f, int e)

{

    error("ami_reverse: Is not implemented");

}

/*******************************************************************************

Turn on underline attribute

Turns on/off the underline attribute.
Note that the attributes can only be set singly.
This is not implemented, but could be done by drawing a line under each
character drawn.

*******************************************************************************/

void ami_underline(FILE* f, int e)

{

    error("ami_underline: Is not implemented");

}

/*******************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void ami_superscript(FILE* f, int e)

{

    error("ami_superscript: Is not implemented");

}

/*******************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void ami_subscript(FILE* f, int e)

{

    error("ami_subscript: Is not implemented");

}

/*******************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.
Note that the attributes can only be set singly.

Italic is causing problems with fixed mode on some fonts, and Windows does not
seem to want to share with me just what the true width of an italic font is
(without taking heroic measures like drawing and testing pixels). So we disable
italic on fixed fonts.

*******************************************************************************/

void ami_italic(FILE* f, int e)

{

    error("ami_italic: Is not implemented");

}

/*******************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

void ami_bold(FILE* f, int e)

{

    error("ami_bold: Is not implemented");
}

/*******************************************************************************

Turn on strikeout attribute

Turns on/off the strikeout attribute.
Note that the attributes can only be set singly.
Not implemented, but strikeout can be done by drawing a line through characters
just placed.

*******************************************************************************/

void ami_strikeout(FILE* f, int e)

{

    error("ami_strikeout: Is not implemented");

}

/*******************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

*******************************************************************************/

void ami_standout(FILE* f, int e)

{

    error("ami_standout: Is not implemented");

}

/*******************************************************************************

Set foreground color

Sets the foreground color from the universal primary code.

*******************************************************************************/

void ami_fcolor(FILE* f, ami_color c)

{

    error("ami_fcolor: Is not implemented");

}

void ami_fcolorc(FILE* f, int r, int g, int b)

{

    error("ami_fcolorc: Is not implemented");

}

/*******************************************************************************

Set foreground color graphical

Sets the foreground color from RGB primaries. The RGB values are scaled from
maxint, so 255 = maxint. This means that if the color resolution ever goes
up, we will be ready.

Fcolor exists as an overload to the text version, but we also provide an
fcolorg for backward compatiblity to the days before overloads.

*******************************************************************************/

void ami_fcolorg(FILE* f, int r, int g, int b)

{

    error("ami_fcolorg: Is not implemented");

}

/*******************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

void ami_bcolor(FILE* f, ami_color c)

{

    error("ami_bcolor: Is not implemented");

}

void ami_bcolorc(FILE* f, int r, int g, int b)

{

    error("ami_bcolorc: Is not implemented");

}

/*******************************************************************************

Set background color graphical

Sets the background color from RGB primaries. The RGB values are scaled from
maxint, so 255 = maxint. This means that if the color resolution ever goes
up, we will be ready.

*******************************************************************************/

void ami_bcolorg(FILE* f, int r, int g, int b)

{

    error("ami_bcolorg: Is not implemented");

}

/*******************************************************************************

Enable/disable automatic scroll and wrap


Enables or disables automatic screen scroll and end of line wrapping. When the
cursor leaves the screen in automatic mode, the following occurs:

up       Scroll down
down     Scroll up
right    Line down, start at left
left     Line up, start at right

These movements can be combined. Leaving the screen right from the lower right
corner will both wrap and scroll up. Leaving the screen left from upper left
will wrap and scroll down.

With auto disabled, no automatic scrolling will occur, and any movement of the
cursor off screen will simply cause the cursor to be undefined. In this
package that means the cursor is off, and no characters are written. On a
real terminal, it simply means that the position is undefined, and could be
anywhere.

*******************************************************************************/

void ami_auto(FILE* f, int e)

{

    error("ami_auto: Is not implemented");

}

/*******************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

*******************************************************************************/

void ami_curvis(FILE* f, int e)

{

    error("ami_curvis: Is not implemented");

}

/*******************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

*******************************************************************************/

int ami_curx(FILE* f)

{

    error("ami_curx: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

int ami_cury(FILE* f)

{

    error("ami_cury: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Get location of cursor in x graphical

Returns the current location of the cursor in x, in pixels.

*******************************************************************************/

int ami_curxg(FILE* f)

{

    error("ami_curxg: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Get location of cursor in y graphical

Returns the current location of the cursor in y, in pixels.

*******************************************************************************/

int ami_curyg(FILE* f)

{

    error("ami_curyg: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Select current screen

Selects one of the screens to set active. If the screen has never been used,
then a new screen is allocated and cleared.
The most common use of the screen selection system is to be able to save the
initial screen to be restored on exit. This is a moot point in this
application, since we cannot save the entry screen in any case.
We allow the screen that is currently active to be reselected. This effectively
forces a screen refresh, which can be important when working on terminals.

*******************************************************************************/

void ami_select(FILE* f, int u, int d)

{

    error("ami_select: Is not implemented");

}

/*******************************************************************************

Write string to current cursor position

Writes a string to the current cursor position, then updates the cursor
position. This acts as a series of write character calls. However, it eliminates
several layers of protocol, and results in much faster write time for
applications that require it.

It is an error to call this routine with auto enabled, since it could exceed
the bounds of the screen.

No control characters or other interpretation is done, and invisible characters
such as controls are not suppressed.

*******************************************************************************/

void ami_wrtstr(FILE* f, char* s)

{

    error("ami_wrtstr: Is not implemented");

}

/*******************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

*******************************************************************************/

void ami_del(FILE* f)

{

    error("ami_del: Is not implemented");

}

/*******************************************************************************

Draw line

Draws a single line in the foreground color.

*******************************************************************************/

void ami_line(FILE* f, int x1, int y1, int x2, int y2)

{

    error("ami_line: Is not implemented");

}

/*******************************************************************************

Draw rectangle

Draws a rectangle in foreground color.

*******************************************************************************/

void ami_rect(FILE* f, int x1, int y1, int x2, int y2)

{

    error("ami_rect: Is not implemented");

}

/*******************************************************************************

Draw filled rectangle

Draws a filled rectangle in foreground color.

*******************************************************************************/

void ami_frect(FILE* f, int x1, int y1, int x2, int y2)

{

    error("ami_frect: Is not implemented");

}

/*******************************************************************************

Draw rounded rectangle

Draws a rounded rectangle in foreground color.

*******************************************************************************/

void ami_rrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys)

{

    error("ami_rrect: Is not implemented");

}

/*******************************************************************************

Draw filled rounded rectangle

Draws a filled rounded rectangle in foreground color.

*******************************************************************************/

void ami_frrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys)

{

    error("ami_frrect: Is not implemented");

}

/*******************************************************************************

Draw ellipse

Draws an ellipse with the current foreground color and line width.

*******************************************************************************/

void ami_ellipse(FILE* f, int x1, int y1, int x2, int y2)

{

    error("ami_ellipse: Is not implemented");

}

/*******************************************************************************

Draw filled ellipse

Draws a filled ellipse with the current foreground color.

*******************************************************************************/

void ami_fellipse(FILE* f, int x1, int y1, int x2, int y2)

{

    error("ami_fellipse: Is not implemented");

}

/*******************************************************************************

Draw arc

Draws an arc in the current foreground color and line width. The containing
rectangle of the ellipse is given, and the start and end angles clockwise from
0 degrees delimit the arc.

Windows takes the start and end delimited by a line extending from the center
of the arc. The way we do the convertion is to project the angle upon a circle
whose radius is the precision we wish to use for the calculation. Then that
point on the circle is found by triangulation.

The larger the circle of precision, the more angles can be represented, but
the trade off is that the circle must not reach the edge of an integer
(-maxint..maxint). That means that the total logical coordinate space must be
shortened by the precision. To find out what division of the circle precis
represents, use cd := precis*2*pi. So, for example, precis = 100 means 628
divisions of the circle.

The end and start points can be negative.
Note that Windows draws arcs counterclockwise, so our start and end points are
swapped.

Negative angles are allowed.

*******************************************************************************/

void ami_arc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

    error("ami_arc: Is not implemented");

}

/*******************************************************************************

Draw filled arc

Draws a filled arc in the current foreground color. The same comments apply
as for the arc function above.

*******************************************************************************/

void ami_farc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

    error("ami_farc: Is not implemented");

}

/*******************************************************************************

Draw filled cord

Draws a filled cord in the current foreground color. The same comments apply
as for the arc function above.

*******************************************************************************/

void ami_fchord(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

    error("ami_fchord: Is not implemented");

}

/*******************************************************************************

Draw filled triangle

Draws a filled triangle in the current foreground color.

*******************************************************************************/

void ami_ftriangle(FILE* f, int x1, int y1, int x2, int y2, int x3, int y3)

{

    error("ami_ftriangle: Is not implemented");

}

/*******************************************************************************

Set pixel

Sets a single logical pixel to the foreground color.

*******************************************************************************/

void ami_setpixel(FILE* f, int x, int y)

{

    error("ami_setpixel: Is not implemented");

}

/*******************************************************************************

Set foreground to overwrite

Sets the foreground write mode to overwrite.

*******************************************************************************/

void ami_fover(FILE* f)

{

    error("ami_fover: Is not implemented");

}

/*******************************************************************************

Set background to overwrite

Sets the background write mode to overwrite.

*******************************************************************************/

void ami_bover(FILE* f)

{

    error("ami_bover: Is not implemented");

}

/*******************************************************************************

Set foreground to invisible

Sets the foreground write mode to invisible.

*******************************************************************************/

void ami_finvis(FILE* f)

{

    error("ami_finvis: Is not implemented");

}

/*******************************************************************************

Set background to invisible

Sets the background write mode to invisible.

*******************************************************************************/

void ami_binvis(FILE* f)

{

    error("ami_binvis: Is not implemented");

}

/*******************************************************************************

Set foreground to xor

Sets the foreground write mode to xor.

*******************************************************************************/

void ami_fxor(FILE* f)

{

    error("ami_fxor: Is not implemented");

}

/*******************************************************************************

Set background to xor

Sets the background write mode to xor.

*******************************************************************************/

void ami_bxor(FILE* f)

{

    error("ami_bxor: Is not implemented");

}

/*******************************************************************************

Set line width

Sets the width of lines and several other figures.

*******************************************************************************/

void ami_linewidth(FILE* f, int w)

{

    error("ami_linewidth: Is not implemented");

}

/*******************************************************************************

Find character size x

Returns the character width.

*******************************************************************************/

int ami_chrsizx(FILE* f)

{

    error("ami_chrsizx: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Find character size y

Returns the character height.

*******************************************************************************/

int ami_chrsizy(FILE* f)

{

    error("ami_chrsizy: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Find number of installed fonts

Finds the total number of installed fonts.

*******************************************************************************/

int ami_fonts(FILE* f)

{

    error("ami_fonts: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Change fonts

Changes the current font to the indicated logical font number.

*******************************************************************************/

void ami_font(FILE* f, int fc)

{

    error("ami_font: Is not implemented");

}

/*******************************************************************************

Find name of font

Returns the name of a font by number.

*******************************************************************************/

void ami_fontnam(FILE* f, int fc, char* fns, int fnsl)

{

    error("ami_fontnam: Is not implemented");

}

/*******************************************************************************

Change font size

Changes the font sizing to match the given character height. The character
and line spacing are changed, as well as the baseline.

*******************************************************************************/

void ami_fontsiz(FILE* f, int s)

{

    error("ami_fontsiz: Is not implemented");

}

/*******************************************************************************

Set character extra spacing y

Sets the extra character space to be added between lines, also referred to
as "ledding".

Not implemented yet.

*******************************************************************************/

void ami_chrspcy(FILE* f, int s)

{

    error("ami_chrspcy: Is not implemented");

}

/*******************************************************************************

Sets extra character space x

Sets the extra character space to be added between characters, referred to
as "spacing".

Not implemented yet.

*******************************************************************************/

void ami_chrspcx(FILE* f, int s)

{

    error("ami_chrspcx: Is not implemented");

}

/*******************************************************************************

Find dots per meter x

Returns the number of dots per meter resolution in x.

*******************************************************************************/

int ami_dpmx(FILE* f)

{

    error("ami_dpmx: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Find dots per meter y

Returns the number of dots per meter resolution in y.

*******************************************************************************/

int ami_dpmy(FILE* f)

{

    error("ami_dpmy: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Find string size in pixels

Returns the number of pixels wide the given string would be, considering
character spacing and kerning.

*******************************************************************************/

int ami_strsiz(FILE* f, const char* s)

{

    error("ami_strsiz: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Find character position in string

Finds the pixel offset to the given character in the string.

*******************************************************************************/

int ami_chrpos(FILE* f, const char* s, int p)

{

    error("ami_chrpos: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Write justified text

Writes a string of text with justification. The string and the width in pixels
is specified. Auto mode cannot be on for this function, nor can it be used on
the system font.

*******************************************************************************/

void ami_writejust(FILE* f, const char* s, int n)

{

    error("ami_writejust: Is not implemented");

}

/*******************************************************************************

Find justified character position

Given a string, a character position in that string, and the total length
of the string in pixels, returns the offset in pixels from the start of the
string to the given character, with justification taken into account.
The model used is that the extra space needed is divided by the number of
spaces, with the fractional part lost.

*******************************************************************************/

int ami_justpos(FILE* f, const char* s, int p, int n)

{

    error("ami_justpos: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Turn on condensed attribute

Turns on/off the condensed attribute. Condensed is a character set with a
shorter baseline than normal characters in the current font.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void ami_condensed(FILE* f, int e)

{

    error("ami_condensed: Is not implemented");

}

/*******************************************************************************

Turn on extended attribute

Turns on/off the extended attribute. Extended is a character set with a
longer baseline than normal characters in the current font.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void ami_extended(FILE* f, int e)

{

    error("ami_extended: Is not implemented");

}

/*******************************************************************************

Turn on extra light attribute

Turns on/off the extra light attribute. Extra light is a character thiner than
light.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void ami_xlight(FILE* f, int e)

{

    error("ami_xlight: Is not implemented");

}

/*******************************************************************************

Turn on light attribute

Turns on/off the light attribute. Light is a character thiner than normal
characters in the currnet font.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void ami_light(FILE* f, int e)

{

    error("ami_light: Is not implemented");

}

/*******************************************************************************

Turn on extra bold attribute

Turns on/off the extra bold attribute. Extra bold is a character thicker than
bold.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void ami_xbold(FILE* f, int e)

{

    error("ami_xbold: Is not implemented");

}

/*******************************************************************************

Turn on hollow attribute

Turns on/off the hollow attribute. Hollow is an embossed or 3d effect that
makes the characters appear sunken into the page.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void ami_hollow(FILE* f, int e)

{

    error("ami_hollow: Is not implemented");

}

/*******************************************************************************

Turn on raised attribute

Turns on/off the raised attribute. Raised is an embossed or 3d effect that
makes the characters appear coming off the page.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void ami_raised(FILE* f, int e)

{

    error("ami_raised: Is not implemented");

}

/*******************************************************************************

Delete picture

Deletes a loaded picture.

*******************************************************************************/

void ami_delpict(FILE* f, int p)

{

    error("ami_delpict: Is not implemented");

}

/*******************************************************************************

Load picture

Loads a picture into a slot of the loadable pictures array.

*******************************************************************************/

void ami_loadpict(FILE* f, int p, char* fn)

{

    error("ami_loadpict: Is not implemented");

}

/*******************************************************************************

Find size x of picture

Returns the size in x of the logical picture.

*******************************************************************************/

int ami_pictsizx(FILE* f, int p)

{

    error("ami_pictsizx: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Find size y of picture

Returns the size in y of the logical picture.

*******************************************************************************/

int ami_pictsizy(FILE* f, int p)

{

    error("ami_pictsizy: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Draw picture

Draws a picture from the given file to the rectangle. The picture is resized
to the size of the rectangle.

Images will be kept in a rotating cache to prevent repeating reloads.

*******************************************************************************/

void ami_picture(FILE* f, int p, int x1, int y1, int x2, int y2)

{

    error("ami_picture: Is not implemented");

}

/*******************************************************************************

Set viewport offset graphical

Sets the offset of the viewport in logical space, in pixels, anywhere from
-maxint to maxint.

*******************************************************************************/

void ami_viewoffg(FILE* f, int x, int y)

{

    error("ami_viewoffg: Is not implemented");

}

/*******************************************************************************

Set viewport scale

Sets the viewport scale in x and y. The scale is a real fraction between 0 and
1, with 1 being 1:1 scaling. Viewport scales are allways smaller than logical
scales, which means that there are more than one logical pixel to map to a
given physical pixel, but never the reverse. This means that pixels are lost
in going to the display, but the display never needs to interpolate pixels
from logical pixels.

Note:

Right now, symmetrical scaling (both x and y scales set the same) are all that
works completely, since we don't presently have a method to warp text to fit
a scaling process. However, this can be done by various means, including
painting into a buffer and transfering asymmetrically, or using outlines.

*******************************************************************************/

void ami_viewscale(FILE* f, float x, float y)

{

    error("ami_viewscale: Is not implemented");

}

/*******************************************************************************

Aquire next input event

Waits for and returns the next event. For now, the input file is ignored, and
the standard input handle allways used.

The event loop for X and the event loop for Petit-Ami are similar. Its not a
coincidence. I designed it after a description I read of the X system in 1997.
Our event loop here is like an event to event translation.

*******************************************************************************/

void ami_event(FILE* f, ami_evtrec* er)

{

    error("ami_event: Is not implemented");

}

/*******************************************************************************

Set timer

Sets an elapsed timer to run, as identified by a timer handle. From 1 to 10
timers can be used. The elapsed time is 32 bit signed, in tenth milliseconds.
This means that a bit more than 24 hours can be measured without using the
sign.

Timers can be set to repeat, in which case the timer will automatically repeat
after completion. When the timer matures, it sends a timer mature event to
the associated input file.

*******************************************************************************/

void ami_timer(FILE* f, /* file to send event to */
              int   i, /* timer handle */
              long  t, /* number of tenth-milliseconds to run */
              int   r) /* timer is to rerun after completion */

{

    error("ami_timer: Is not implemented");

}

/*******************************************************************************

Kill timer

Kills a given timer, by it's id number. Only repeating timers should be killed.

*******************************************************************************/

void ami_killtimer(FILE*  f, /* file to kill timer on */
                  int    i) /* handle of timer */

{

    error("ami_killtimer: Is not implemented");

}

/*******************************************************************************

Set/kill framing timer

Sets the framing timer. The frame timer is a reserved timer that here counts
off 1/60 second heartbeats, an average frame rate. On installations where this
is possible, it actually gets tied to the real screen refresh at the start
of the blanking interval.

*******************************************************************************/

void ami_frametimer(FILE* f, int e)

{

    error("ami_frametimer: Is not implemented");

}

/*******************************************************************************

Set automatic hold state

Sets the state of the automatic hold flag. Automatic hold is used to hold
programs that exit without having received a "terminate" signal from gralib.
This exists to allow the results of gralib unaware programs to be viewed after
termination, instead of exiting an distroying the window. This mode works for
most circumstances, but an advanced program may want to exit for other reasons
than being closed by the system bar. This call can turn automatic holding off,
and can only be used by an advanced program, so fufills the requirement of
holding gralib unaware programs.

*******************************************************************************/

void ami_autohold(int e)

{

    error("ami_autohold: Is not implemented");

}

/*******************************************************************************

Return number of mice

Returns the number of mice implemented. Windows supports only one mouse.

*******************************************************************************/

int ami_mouse(FILE* f)

{

    error("ami_mouse: Is not implemented");

    return (0); /* this just shuts up compiler */

}

/*******************************************************************************

Return number of buttons on mouse

Returns the number of buttons on the mouse. There is only one mouse in this
version.

*******************************************************************************/

int ami_mousebutton(FILE* f, int m)

{

    error("ami_mousebutton: Is not implemented");

    return (0); /* this just shuts up compiler */

}

/*******************************************************************************

Return number of joysticks

Return number of joysticks attached.

*******************************************************************************/

int ami_joystick(FILE* f)

{

    error("ami_joystick: Is not implemented");

    return (0); /* this just shuts up compiler */

}

/*******************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.

*******************************************************************************/

int ami_joybutton(FILE* f, int j)

{

    error("ami_joybutton: Is not implemented");

    return (0); /* this just shuts up compiler */

}

/*******************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodimensional
joystick can be considered a slider without positional meaning.

*******************************************************************************/

int ami_joyaxis(FILE* f, int j)

{

    error("ami_joyaxis: Is not implemented");

    return (0); /* this just shuts up compiler */

}

/*******************************************************************************

Set tab graphical

Sets a tab at the indicated pixel number.

*******************************************************************************/

void ami_settabg(FILE* f, int t)

{

    error("ami_settabg: Is not implemented");

}

/*******************************************************************************

Set tab

Sets a tab at the indicated collumn number.

*******************************************************************************/

void ami_settab(FILE* f, int t)

{

    error("ami_settab: Is not implemented");

}

/*******************************************************************************

Reset tab graphical

Resets the tab at the indicated pixel number.

*******************************************************************************/

void ami_restabg(FILE* f, int t)

{

    error("ami_restabg: Is not implemented");

}

/*******************************************************************************

Reset tab

Resets the tab at the indicated collumn number.

*******************************************************************************/

void ami_restab(FILE* f, int t)

{

    error("ami_restab: Is not implemented");

}

/*******************************************************************************

Clear all tabs

Clears all the set tabs. This is usually done prior to setting a custom tabbing
arrangement.

*******************************************************************************/

void ami_clrtab(FILE* f)

{

    error("ami_clrtab: Is not implemented");

}

/*******************************************************************************

Find number of function keys

Finds the total number of function, or general assignment keys. Currently, we
just implement the 12 unshifted PC function keys. We may add control and shift
function keys as well.

*******************************************************************************/

int ami_funkey(FILE* f)

{

    error("ami_funkey: Is not implemented");

    return (0); /* this just shuts up compiler */

}

/*******************************************************************************

Set window title

Sets the title of the current window.

*******************************************************************************/

void ami_title(FILE* f, char* ts)

{

    error("ami_title: Is not implemented");

}

/*******************************************************************************

Open window

Opens a window to an input/output pair. The window is opened and initalized.
If a parent is provided, the window becomes a child window of the parent.
The window id can be from 1 to ss_maxhdl, but the input and output file ids
of 1 and 2 are reserved for the input and output files, and cannot be used
directly. These ids will be be opened as a pair anytime the "_input" or
"_output" file names are seen.

*******************************************************************************/

void ami_openwin(FILE** infile, FILE** outfile, FILE* parent, int wid)

{

    error("ami_openwin: Is not implemented");

}

/*******************************************************************************

Size buffer pixel

Sets or resets the size of the buffer surface, in pixel units.

*******************************************************************************/

void ami_sizbufg(FILE* f, int x, int y)

{

    error("ami_sizbufg: Is not implemented");

}

/*******************************************************************************

Size buffer in characters

Sets or resets the size of the buffer surface, in character counts.

*******************************************************************************/

void ami_sizbuf(FILE* f, int x, int y)

{

    error("ami_sizbuf: Is not implemented");

}

/*******************************************************************************

Enable/disable buffered mode

Enables or disables surface buffering. If screen buffers are active, they are
freed.

*******************************************************************************/

void ami_buffer(FILE* f, int e)

{

    error("ami_buffer: Is not implemented");

}

/*******************************************************************************

Activate/distroy menu

Accepts a menu list, and sets the menu active. If there is already a menu
active, that is replaced. If the menu list is nil, then the active menu is
deleted.

*******************************************************************************/

void ami_menu(FILE* f, ami_menuptr m)

{

    error("ami_menu: Is not implemented");

}

/*******************************************************************************

Enable/disable menu entry

Enables or disables a menu entry by id. The entry is set to grey if disabled,
and will no longer send messages.

*******************************************************************************/

void ami_menuena(FILE* f, int id, int onoff)

{

    error("ami_menuena: Is not implemented");

}

/*******************************************************************************

select/deselect menu entry

Selects or deselects a menu entry by id. The entry is set to checked if
selected, with no check if not.

*******************************************************************************/

void ami_menusel(FILE* f, int id, int select)

{

    error("ami_menusel: Is not implemented");

}

/*******************************************************************************

Bring window to front of the Z order

Brings the indicated window to the front of the Z order.

*******************************************************************************/

void ami_front(FILE* f)

{

    error("ami_front: Is not implemented");

}

/*******************************************************************************

Puts window to the back of the Z order

Puts the indicated window to the back of the Z order.

*******************************************************************************/

void ami_back(FILE* f)

{

    error("ami_back: Is not implemented");

}

/*******************************************************************************

Get window size graphical

Gets the onscreen window size.

*******************************************************************************/

void ami_getsizg(FILE* f, int* x, int* y)

{

    error("ami_getsizg: Is not implemented");

}

/*******************************************************************************

Get window size character

Gets the onscreen window size, in character terms. If the window has a parent,
the demensions are converted to the current character size there. Otherwise,
the pixel based dementions are returned. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void ami_getsiz(FILE* f, int* x, int* y)

{

    error("ami_getsiz: Is not implemented");

}

/*******************************************************************************

Set window size graphical

Sets the onscreen window to the given size.

*******************************************************************************/

void ami_setsizg(FILE* f, int x, int y)

{

    error("ami_setsizg: Is not implemented");

}

/*******************************************************************************

Set window size character

Sets the onscreen window size, in character terms. If the window has a parent,
the demensions are converted to the current character size there. Otherwise,
the pixel based dementions are used. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void ami_setsiz(FILE* f, int x, int y)

{

    error("ami_setsiz: Is not implemented");

}

/*******************************************************************************

Set window position graphical

Sets the onscreen window to the given position in its parent.

*******************************************************************************/

void ami_setposg(FILE* f, int x, int y)

{

    error("ami_setposg: Is not implemented");

}

/*******************************************************************************

Set window position character

Sets the onscreen window position, in character terms. If the window has a
parent, the demensions are converted to the current character size there.
Otherwise, pixel based demensions are used. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void ami_setpos(FILE* f, int x, int y)

{

    error("ami_setpos: Is not implemented");

}

/*******************************************************************************

Get screen size graphical

Gets the total screen size.

*******************************************************************************/

void ami_scnsizg(FILE* f, int* x, int* y)

{

    error("ami_scnsizg: Is not implemented");

}

/*******************************************************************************

Find window size from client

Finds the window size, in parent terms, needed to result in a given client
window size.

Note: this routine should be able to find the minimum size of a window using
the given style, and return the minimums if the input size is lower than this.
This does not seem to be obvious under Windows.

Do we also need a menu style type ?

*******************************************************************************/

void ami_winclient(FILE* f, int cx, int cy, int* wx, int* wy,
               ami_winmodset msset)

{

    error("ami_winclient: Is not implemented");

}

void ami_winclientg(FILE* f, int cx, int cy, int* wx, int* wy,
                ami_winmodset ms)

{

    error("ami_winclientg: Is not implemented");

}

/*******************************************************************************

Get screen size character

Gets the desktopsize, in character terms. Returns the pixel size of the screen
This occurs because the desktop does not have a fixed character aspect, so we
make one up, and our logical character is "one pixel" high and wide. It works
because it can only be used as a relative measurement.

*******************************************************************************/

void ami_scnsiz(FILE* f, int* x, int* y)

{

    error("ami_scnsiz: Is not implemented");

}

/*******************************************************************************

Enable or disable window frame

Turns the window frame on and off.

*******************************************************************************/

void ami_frame(FILE* f, int e)

{

    error("ami_frame: Is not implemented");

}

/*******************************************************************************

Enable or disable window sizing

Turns the window sizing on and off.

*******************************************************************************/

void ami_sizable(FILE* f, int e)

{

    error("ami_sizable: Is not implemented");

}

/*******************************************************************************

Enable or disable window system bar

Turns the system bar on and off.

*******************************************************************************/

void ami_sysbar(FILE* f, int e)

{

    error("ami_sysbar: Is not implemented");

}

/*******************************************************************************

Create standard menu

Creates a standard menu set. Given a set of standard items selected in a set,
and a program added menu list, creates a new standard menu.

On this windows version, the standard lists are:

file edit <program> window help

That is, all of the standard items are sorted into the lists at the start and
end of the menu, then the program selections placed in the menu.

*******************************************************************************/

void ami_stdmenu(ami_stdmenusel sms, ami_menuptr* sm, ami_menuptr pm)

{

    error("ami_stdmenu: Is not implemented");

}

/*******************************************************************************

Kill widget

Removes the widget by id from the window.

*******************************************************************************/

void ami_killwidget(FILE* f, int id)

{

    error("ami_killwidget: Is not implemented");

}

/*******************************************************************************

Select/deselect widget

Selects or deselects a widget.

*******************************************************************************/

void ami_selectwidget(FILE* f, int id, int e)

{

    error("ami_selectwidget: Is not implemented");

}

/*******************************************************************************

Enable/disable widget

Enables or disables a widget.

*******************************************************************************/

void ami_enablewidget(FILE* f, int id, int e)

{

    error("ami_enablewidget: Is not implemented");

}

/*******************************************************************************

Get widget text

Retrives the text from a widget. The widget must be one that contains text.
It is an error if this call is used on a widget that does not contain text.
This error is currently unchecked.

*******************************************************************************/

void ami_getwidgettext(FILE* f, int id, char* s, int sl)

{

    error("ami_getwidgettext: Is not implemented");

}

/*******************************************************************************

put edit box text

Places text into an edit box.

*******************************************************************************/

void ami_putwidgettext(FILE* f, int id, char* s)

{

    error("ami_putwidgettext: Is not implemented");

}

/*******************************************************************************

Resize widget

Changes the size of a widget.

*******************************************************************************/

void ami_sizwidgetg(FILE* f, int id, int x, int y)

{

    error("ami_sizewidgetg: Is not implemented");

}

/*******************************************************************************

Reposition widget

Changes the parent position of a widget.

*******************************************************************************/

void ami_poswidgetg(FILE* f, int id, int x, int y)

{

    error("ami_poswidgetg: Is not implemented");

}

/*******************************************************************************

Place widget to back of Z order

*******************************************************************************/

void ami_backwidget(FILE* f, int id)

{

    error("ami_backwidget: Is not implemented");

}

/*******************************************************************************

Place widget to back of Z order

*******************************************************************************/

void ami_frontwidget(FILE* f, int id)

{

    error("ami_frontwidget: Is not implemented");

}

/*******************************************************************************

Find minimum/standard button size

Finds the minimum size for a button. Given the face string, the minimum size of
a button is calculated and returned.

*******************************************************************************/

void ami_buttonsizg(FILE* f, char* s, int* w, int* h)

{

    error("ami_buttonsizg: Is not implemented");

}

void ami_buttonsiz(FILE* f, char* s, int* w, int* h)

{

    error("ami_buttonsiz: Is not implemented");

}

/*******************************************************************************

Create button

Creates a standard button within the specified rectangle, on the given window.

*******************************************************************************/

void ami_buttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    error("ami_buttong: Is not implemented");

}

void ami_button(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    error("ami_button: Is not implemented");

}

/*******************************************************************************

Find minimum/standard checkbox size

Finds the minimum size for a checkbox. Given the face string, the minimum size of
a checkbox is calculated and returned.

*******************************************************************************/

void ami_checkboxsizg(FILE* f, char* s, int* w, int* h)

{

    error("ami_checkboxsizg: Is not implemented");

}

void ami_checkboxsiz(FILE* f, char* s,  int* w, int* h)

{

    error("ami_checkboxsiz: Is not implemented");

}

/*******************************************************************************

Create checkbox

Creates a standard checkbox within the specified rectangle, on the given
window.

*******************************************************************************/

void ami_checkboxg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    error("ami_checkboxg: Is not implemented");

}

void ami_checkbox(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    error("ami_checkbox: Is not implemented");

}

/*******************************************************************************

Find minimum/standard radio button size

Finds the minimum size for a radio button. Given the face string, the minimum
size of a radio button is calculated and returned.

*******************************************************************************/

void ami_radiobuttonsizg(FILE* f, char* s, int* w, int* h)

{

    error("ami_radiobuttonsizg: Is not implemented");

}

void ami_radiobuttonsiz(FILE* f, char* s, int* w, int* h)

{

    error("ami_radiobuttonsiz: Is not implemented");

}

/*******************************************************************************

Create radio button

Creates a standard radio button within the specified rectangle, on the given
window.

*******************************************************************************/

void ami_radiobuttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    error("ami_radiobuttong: Is not implemented");

}

void ami_radiobutton(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    error("ami_radiobutton: Is not implemented");

}

/*******************************************************************************

Find minimum/standard group size

Finds the minimum size for a group. Given the face string, the minimum
size of a group is calculated and returned.

*******************************************************************************/

void ami_groupsizg(FILE* f, char* s, int cw, int ch, int* w, int* h,
               int* ox, int* oy)

{

    error("ami_groupsizg: Is not implemented");

}

void ami_groupsiz(FILE* f, char* s, int cw, int ch, int* w, int* h,
              int* ox, int* oy)

{

    error("ami_groupsiz: Is not implemented");

}

/*******************************************************************************

Create group box

Creates a group box, which is really just a decorative feature that gererates
no messages. It is used as a background for other widgets.

*******************************************************************************/

void ami_groupg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    error("ami_groupg: Is not implemented");

}

void ami_group(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    error("ami_group: Is not implemented");

}

/*******************************************************************************

Create background box

Creates a background box, which is really just a decorative feature that
generates no messages. It is used as a background for other widgets.

*******************************************************************************/

void ami_backgroundg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    error("ami_backgroundg: Is not implemented");

}

void ami_background(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    error("ami_background: Is not implemented");

}

/*******************************************************************************

Find minimum/standard vertical scrollbar size

Finds the minimum size for a vertical scrollbar. The minimum size of a vertical
scrollbar is calculated and returned.

*******************************************************************************/

void ami_scrollvertsizg(FILE* f, int* w, int* h)

{

    error("ami_scrollvertsizg: Is not implemented");

}

void ami_scrollvertsiz(FILE* f, int* w, int* h)

{

    error("ami_scrollvertsiz: Is not implemented");

}

/*******************************************************************************

Create vertical scrollbar

Creates a vertical scrollbar.

*******************************************************************************/

void ami_scrollvertg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    error("ami_scrollvertg: Is not implemented");

}

void ami_scrollvert(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    error("ami_scrollvert: Is not implemented");

}

/*******************************************************************************

Find minimum/standard horizontal scrollbar size

Finds the minimum size for a horizontal scrollbar. The minimum size of a
horizontal scrollbar is calculated and returned.

*******************************************************************************/

void ami_scrollhorizsizg(FILE* f, int* w, int* h)

{

    error("ami_scrolhorizsizg: Is not implemented");

}

void ami_scrollhorizsiz(FILE* f, int* w, int* h)

{

    error("ami_scrollhorizsiz: Is not implemented");

}

/*******************************************************************************

Create horizontal scrollbar

Creates a horizontal scrollbar.

*******************************************************************************/

void ami_scrollhorizg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    error("ami_scrollhorizg: Is not implemented");

}

void ami_scrollhoriz(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    error("ami_scrollhoriz: Is not implemented");

}

/*******************************************************************************

Set scrollbar position

Sets the current position of a scrollbar slider.

*******************************************************************************/

void ami_scrollpos(FILE* f, int id, int r)

{

    error("ami_scrollpos: Is not implemented");

}

/*******************************************************************************

Set scrollbar size

Sets the current size of a scrollbar slider.

*******************************************************************************/

void ami_scrollsiz(FILE* f, int id, int r)

{

    error("ami_scrollsiz: Is not implemented");

}

/*******************************************************************************

Find minimum/standard number select box size

Finds the minimum size for a number select box. The minimum size of a number
select box is calculated and returned.

*******************************************************************************/

void ami_numselboxsizg(FILE* f, int l, int u, int* w, int* h)

{

    error("ami_numselboxsizg: Is not implemented");

}

void ami_numselboxsiz(FILE* f, int l, int u, int* w, int* h)

{

    error("ami_numselboxsiz: Is not implemented");

}

/*******************************************************************************

Create number selector

Creates an up/down control for numeric selection.

*******************************************************************************/

void ami_numselboxg(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id)

{

    error("ami_numselboxg: Is not implemented");

}

void ami_numselbox(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id)

{

    error("ami_numselbox: Is not implemented");

}

/*******************************************************************************

Find minimum/standard edit box size

Finds the minimum size for an edit box. Given a sample face string, the minimum
size of an edit box is calculated and returned.

*******************************************************************************/

void ami_editboxsizg(FILE* f, char* s, int* w, int* h)

{

    error("ami_editboxsizg: Is not implemented");

}

void ami_editboxsiz(FILE* f, char* s, int* w, int* h)

{

    error("ami_editboxsiz: Is not implemented");

}

/*******************************************************************************

Create edit box

Creates single line edit box

*******************************************************************************/

void ami_editboxg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    error("ami_editboxg: Is not implemented");

}

void ami_editbox(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    error("ami_editbox: Is not implemented");

}

/*******************************************************************************

Find minimum/standard edit box size

Finds the minimum size for an edit box. Given a sample face string, the minimum
size of an edit box is calculated and returned.

*******************************************************************************/

void ami_progbarsizg(FILE* f, int* w, int* h)

{

    error("ami_progbarsizg: Is not implemented");

}

void ami_progbarsiz(FILE* f, int* w, int* h)

{

    error("ami_progbarsiz: Is not implemented");

}

/*******************************************************************************

Create progress bar

Creates a progress bar.

*******************************************************************************/

void ami_progbarg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    error("ami_progbarg: Is not implemented");

}

void progbar(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    error("ami_progbar: Is not implemented");

}

/*******************************************************************************

Set progress bar position

Sets the position of a progress bar, from 0 to maxint.

*******************************************************************************/

void ami_progbarpos(FILE* f, int id, int pos)

{

    error("ami_progbarpos: Is not implemented");

}

/*******************************************************************************

Find minimum/standard list box size

Finds the minimum size for an list box. Given a string list, the minimum size
of an list box is calculated and returned.

Windows listboxes pretty much ignore the size given. If you allocate more space
than needed, it will only put blank lines below if enough space for an entire
line is present. If the size does not contain exactly enough to display the
whole line list, the box will colapse to a single line with an up/down
control. The only thing that is garanteed is that the box will fit within the
specified rectangle, one way or another.

*******************************************************************************/

void ami_boxsizg(FILE* f, ami_strptr sp, int* w, int* h)

{

    error("ami_boxsizg: Is not implemented");

}

void ami_boxsiz(FILE* f, ami_strptr sp, int* w, int* h)

{

    error("ami_boxsiz: Is not implemented");

}

/*******************************************************************************

Create list box

Creates a list box. Fills it with the string list provided.

*******************************************************************************/

void ami_boxg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)

{

    error("ami_boxg: Is not implemented");

}

void ami_box(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)

{

    error("ami_box: Is not implemented");

}

/*******************************************************************************

Find minimum/standard dropbox size

Finds the minimum size for a dropbox. Given the face string, the minimum size of
a dropbox is calculated and returned, for both the "open" and "closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

*******************************************************************************/

void ami_dropboxsizg(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    error("ami_dropboxsizg: Is not implemented");

}

void ami_dropboxsiz(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    error("ami_dropboxsiz: Is not implemented");

}

/*******************************************************************************

Create dropdown box

Creates a dropdown box. Fills it with the string list provided.

*******************************************************************************/

void ami_dropboxg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)

{

    error("ami_dropboxg: Is not implemented");

}

void ami_dropbox(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)

{

    error("ami_dropbox: Is not implemented");

}

/*******************************************************************************

Find minimum/standard drop edit box size

Finds the minimum size for a drop edit box. Given the face string, the minimum
size of a drop edit box is calculated and returned, for both the "open" and
"closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

*******************************************************************************/

void ami_dropeditboxsizg(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    error("ami_dropeditboxsizg: Is not implemented");

}

void ami_dropeditboxsiz(FILE* f, ami_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    error("ami_dropeditboxsiz: Is not implemented");

}

/*******************************************************************************

Create dropdown edit box

Creates a dropdown edit box. Fills it with the string list provided.

We need to subclass a mode where a return selects the current contents of the
box.

*******************************************************************************/

void ami_dropeditboxg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)

{

    error("ami_dropeditboxg: Is not implemented");

}

void ami_dropeditbox(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp, int id)

{

    error("ami_dropeditbox: Is not implemented");

}

/*******************************************************************************

Find minimum/standard horizontal slider size

Finds the minimum size for a horizontal slider. The minimum size of a horizontal
slider is calculated and returned.

*******************************************************************************/

void ami_slidehorizsizg(FILE* f, int * w, int* h)

{

    error("ami_slidehorizsizg: Is not implemented");

}

void ami_slidehorizsiz(FILE* f, int* w, int* h)

{

    error("ami_slidehorizsiz: Is not implemented");

}

/*******************************************************************************

Create horizontal slider

Creates a horizontal slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

*******************************************************************************/

void ami_slidehorizg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

    error("ami_slidehorizg: Is not implemented");

}

void ami_slidehoriz(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

    error("ami_slidehoriz: Is not implemented");

}

/*******************************************************************************

Find minimum/standard vertical slider size

Finds the minimum size for a vertical slider. The minimum size of a vertical
slider is calculated and returned.

*******************************************************************************/

void ami_slidevertsizg(FILE* f, int* w, int* h)

{

    error("ami_slidevertsizg: Is not implemented");

}

void ami_slidevertsiz(FILE* f, int* w, int* h)

{

    error("ami_slidevertsiz: Is not implemented");

}

/*******************************************************************************

Create vertical slider

Creates a vertical slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

*******************************************************************************/

void ami_slidevertg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

    error("ami_slidevertg: Is not implemented");

}

void ami_slidevert(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

    error("ami_slidevert: Is not implemented");

}

/*******************************************************************************

Find minimum/standard tab bar size

Finds the minimum size for a tab bar. The minimum size of a tab bar is
calculated and returned.

*******************************************************************************/

void ami_tabbarsizg(FILE* f, ami_tabori tor, int cw, int ch, int* w, int* h,
                int* ox, int* oy)

{

    error("ami_tabbarsizg: Is not implemented");

}

void ami_tabbarsiz(FILE* f, ami_tabori tor, int cw, int ch, int * w, int* h,
               int* ox, int* oy)

{

    error("ami_tabbarsiz: Is not implemented");

}

/*******************************************************************************

Find client from tabbar size

Given a tabbar size and orientation, this routine gives the client size and
offset. This is used where the tabbar size is fixed, but the client area is
flexible.

*******************************************************************************/

void ami_tabbarclientg(FILE* f, ami_tabori tor, int w, int h, int* cw, int* ch,
                   int* ox, int* oy)

{

    error("ami_tabbarclientg: Is not implemented");

}

void ami_tabbarclient(FILE* f, ami_tabori tor, int w, int h, int* cw, int* ch,
                  int* ox, int* oy)

{

    error("ami_tabbarclient: Is not implemented");

}

/*******************************************************************************

Create tab bar

Creates a tab bar with the given orientation.

Bug: has strange overwrite mode where when the widget is first created, it
allows itself to be overwritten by the main window. This is worked around by
creating and distroying another widget.

*******************************************************************************/

void ami_tabbarg(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp,
             ami_tabori tor, int id)

{

    error("ami_tabbarg: Is not implemented");

}

void ami_tabbar(FILE* f, int x1, int y1, int x2, int y2, ami_strptr sp,
            ami_tabori tor, int id)

{

    error("ami_tabbar: Is not implemented");

}

/*******************************************************************************

Set tabbar current select

Sets the current tab selected in a tabbar. The select is the ordinal number
of the tab.

*******************************************************************************/

void ami_tabsel(FILE* f, int id, int tn)

{

    error("ami_tabsel: Is not implemented");

}

/*******************************************************************************

Output message dialog

Outputs a message dialog with the given title and message strings.

*******************************************************************************/

void ami_alert(char* title, char* message)

{

    error("ami_alert: Is not implemented");

}

/*******************************************************************************

Display choose color dialog

Presents the choose color dialog, then returns the resulting color.

Bug: does not take the input color as the default.

*******************************************************************************/

void ami_querycolor(int* r, int* g, int* b)

{

    error("ami_querycolor: Is not implemented");

}

/*******************************************************************************

Display choose file dialog for open

Presents the choose file dialog, then returns the file string as a dynamic
string. The default string passed in is presented in the dialog, and a new
string replaces it. The caller is responsible for disposing of the input
string and the output string.

If a wildcard is passed as the default, this will be used to filter the files
in the current directory into a list.

If the operation is cancelled, then a null string will be returned.

*******************************************************************************/

void ami_queryopen(char* s, int sl)

{

    error("ami_queryopen: Is not implemented");

}

/*******************************************************************************

Display choose file dialog for save

Presents the choose file dialog, then returns the file string as a dynamic
string. The default string passed in is presented in the dialog, and a new
string replaces it. The caller is responsible for disposing of the input
string and the output string.

If a wildcard is passed as the default, this will be used to filter the files
in the current directory into a list.

If the operation is cancelled, then a null string will be returned.

*******************************************************************************/

void ami_querysave(char* s, int sl)

{

    error("ami_querysave: Is not implemented");

}

/*******************************************************************************

Display choose find text dialog

Presents the choose find text dialog, then returns the resulting string.
A find/replace option set can be specified. The parameters are "flow through",
meaning that you set them before the call, and they may or may not be changed
from these defaults after the call. In addition, the parameters are used to
set the dialog.

The string that is passed in is discarded without complaint. It is up to the
caller to dispose of it properly.

Bug: should return null string on cancel. Unlike other dialogs, windows
provides no indication of if the cancel button was pushed. To do this, we
would need to hook (or subclass) the find dialog.

After note: tried hooking the window. The issue is that the cancel button is
just a simple button that gets pressed. Trying to rely on the button id
sounds very system dependent, since that could change. One method might be
to retrive the button text, but this is still fairly system dependent. We
table this issue until later.

*******************************************************************************/

void ami_queryfind(char* s, int sl, ami_qfnopts* opt)

{

    error("ami_queryfind: Is not implemented");

}

/*******************************************************************************

Display choose replace text dialog

Presents the choose replace text dialog, then returns the resulting string.
A find/replace option set can be specified. The parameters are "flow through",
meaning that you set them before the call, and they may or may not be changed
from these defaults after the call. In addition, the parameters are used to
set the dialog.

The string that is passed in is discarded without complaint. It is up to the
caller to dispose of it properly.

Bug: See comment, queryfind.

*******************************************************************************/

void ami_queryfindrep(char* s, int sl, char* r, int rl, ami_qfropts* opt)

{

    error("ami_queryfindrep: Is not implemented");

}

/*******************************************************************************

Display choose font dialog

Presents the choose font dialog, then returns the resulting logical font
number, size, foreground color, background color, and effects (in a special
effects set for this routine).

The parameters are "flow through", meaning that they should be set to their
defaults before the call, and changes are made, then updated to the parameters.
During the routine, the state of the parameters given are presented to the
user as the defaults.

*******************************************************************************/

void ami_queryfont(FILE* f, int* fc, int* s, int* fr, int* fg, int* fb,
               int* br, int* bg, int* bb, ami_qfteffects* effect)

{

    error("ami_queryfont: Is not implemented");

}
