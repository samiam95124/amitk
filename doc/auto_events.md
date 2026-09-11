## An automated event file format

We need a way to automatically provide events in test files. This is a text
format file, that describes all of the possible event types in 
include/graphics.h. All of the events will appear by name after the prefix
ami_et, followed by the parameters they have, if any. Each event will occupy
a single line.

For example:

char 'h'

signifies to generate an ami_etchar event with the character value set to 'h'.

Each event in series will be provided to the program, as soon as it asks for
an event. The events in the file will be used up until the end of the file or
a "sync" marker occurs.

## Sync lines

A sync line consists of:

sync frame[.fraction]

The frame number is the test frame number. The fraction may or may not exist,
and specifies the frame fraction if it exists.

The action of a sync line is to wait until that frame, and the frame fraction,
if it exists, is active before continuing. Then events will be read from the
file. In this way, the events are only given when the desired frame is active.

The routine event() in the tests will be replaced by auto_event(). This will
just call regular Ami event() if no events in the event file are ready to be
processed. This would occur if any of the following are true:

1. There is no event file.
2. The event file is at the end.
3. The last read statement in the event file is for a frame or frame.fraction
that has not happened yet.

If there IS a frame sync, and it does not match the current frame.fraction,
nothing is read from the event file. otherwise, if the file is not at end,
the next statement is read from the file, a line at a time. Then, then the
event statement is acted on. This will be either:

1. An event, which will cause an event record to be created and returned to the
caller.
2. A sync, which will be stored to filter subsequent calls.

## Example of use:

In the mouse movement test, we will give a sync line that matches the start of
the mouse movement test frame number. The statements in the file will wait
until the mouse movement test starts. Then several mouse movements will appear
in the file, specifying the coordinates of the mouse in x,y. Then mouse button
events will be sent.

At each step, a frame fraction will be recorded.

The file will either end, or a new frame for a new test will appear.

## Finding frame numbers

Obviously, the test must be run once just to find the frame numbers. This would
be necessary each time the test is changed. Similarly, a simulated mouse click
of (say) a widget control would have to be recalibrated each time the code of
either the test, or in some cases, graphics.c/widget.c changes.


## As built

The module is tests/auto_event.c, linked into the picture tests and, built
with AUTO_EVENT_TERMINAL for the terminal's event set, into the terminal
tests. The file is named after its test, tests/graphics_test.evt,
tests/terminal_test.evt, tests/window_test.evt or tests/widget_test.evt, and
is committed beside it as an input. A run given a
capture file takes the event file from the capture file's directory, since
the regression runs the terminal tests from tests/terminal; terminal_testc,
the same test on the console library, takes terminal_test's file. Lines hold one statement each, '#' starts a comment:

    <event> [parameters]    an event, named as in include/graphics.h without
                            the ami_et prefix, its parameters in the order of
                            the event record: char 'h', moumovg 1 400 300,
                            mouba 1 1, enter, button 5
    sync <frame>[.<step>]   hold the events after it until the test is on that
                            frame, and that step of it if given
    window <id>             the window the events that follow carry (1 to start)
    wait <ms>               let the program run for that long, its events its
                            own, then a step: what time did, a progress bar
    keyboardoff, mouseoff, joystickoff
                            drop the events of the real keyboard, mouse or
                            joystick, so a device on the desk cannot join a test
                            of its events from the file; keyboardon, mouseon and
                            joystickon let them through again
    mousenum n, joysticknum n
                            the count of mice or joysticks the test is told of,
                            through auto_mouse() and auto_joystick(), which the
                            test uses in place of ami_mouse() and ami_joystick()
    mousebutton n, joybutton n, joyaxis n
                            the buttons of every mouse, and the buttons and axes
                            of every joystick, through auto_mousebutton(),
                            auto_joybutton() and auto_joyaxis() the same way

The device settings go at the start of the file, before any sync, so they
hold for the run. Everything is done on the test's side: a device set aside
has its events skipped on their way from ami_event(), and a count the file
sets is answered by the module instead of Ami.

A number is written plain, or as max, -max, max/n or -max/n, the largest
value of the type and fractions of it, which is the range of a joystick's
axes.

The test names the file with auto_event_name(), reports its position with
auto_event_frame() as it numbers frames and steps, and asks for events with
auto_event() in place of ami_event(). auto_event_ready() says whether the file
has an event ready for the frame at hand: an automatic run, which has no user
to fall back on, enters a pattern's event loop only while that is so, and
captures a step after each event it applies. Each pattern's events end with
the enter that ends the pattern for a user.

Frame numbers advance at the start of each pattern (frmnext in graphics_test),
so the title while a pattern runs, the steps recorded within it and the
capture at its end all carry the same number, and that is the number a sync
names. "graphics_test auto" reads the file; "graphics_test events" reads it
in an interactive run as well. With AUTO_EVENT_TRACE set in the environment
each event given is reported on stderr with the frame and step it went to,
which is how the numbers for a new file are found. A file the run does not use
up, because a sync was never reached or events were never asked for, is
reported on stderr at the end.

Each capture carries its label, "frame N" or "frame N.S", as the PNG title
(screen_capture_label, before screen_capture), so testviewer's shifted arrows
step between whole frames in a picture stream the way they do in a text
standard.

## The seat

Where the display carries a seat rig, which the Wayland layer does (its
PD_INPUT fifo, made for headless compositors) and the X module does through
the XTest extension, the mouse and key events of the file are not handed to
the program but put in at the seat, as if a person made them. A move goes to the point named, in the client area of the main
window; a button press or release goes to the pointer's place; a key goes to
whatever holds the keyboard focus. The display routes them the way it routes
a real seat's input: the hit test finds the leaf window under the point, a
widget's face or the client area, with hover, focus and crossings as for a
person, and the program gets what the widget sends. So

    moumovg 1 264 251
    mouba 1 1
    moubd 1 1

presses whatever is at 264,251, and widget_test's file works its buttons,
scroll bars and number boxes that way, the coordinates read off the pictures
of the standard, which is the calibration doc/auto_events.md anticipated.
Keys are given as keycodes, so a character is a key of the US layout: letters,
digits and the unshifted punctuation, with a held shift for the rest.

After each such line the module waits a fixed moment, 200 ms, handing the
program every event the line provokes, and then a beat: auto_event_step()
says one when the line has run its course, and that is when a test captures
a step. The moment is fixed rather than a quiet interval because a program
with the frame timer on is never quiet.

Focus follows the seat's clicks as it does a person's: a press leaves the
keyboard with the widget pressed, so a file clicks the main window before the
return that is to end a chapter, and clicks a number box's field before the
return that is to send its value. The module opens the seat itself, a fifo
of its own named to the display in PD_INPUT, on the first mouse or key
event; where there is no rig, the remote client, the terminal, the
framebuffer, or PD_INPUT is already someone else's, the events go to the
program directly as before, and the widget and menu events always do. While
the rig's fifo is open the rig holds the seat: the display ignores the
compositor's own pointer and keyboard, crossings and focus included, so a
run on a live desktop is not joined by the hand on the desk and the focus a
click gave a widget stays put. With the seat in use keyboardoff and mouseoff
are moot.

A frame the test passes without running its pattern, as a selected range
does, leaves the frame's events in the file: they are skipped to the next
sync, and counted at the end.

### The X seat

The X module reads the same fifo and the same commands, and puts them in
at the X server through XTest, so that they arrive as a person's would,
with the server's own hit testing, crossings and focus. The point named is
in the X window itself, which is the client area, so the event module adds
no frame offset there; a key is preceded by giving the target window the
input focus, since the server sends keys to the focus and a test window
does not have it by right. Two things XTest cannot do: it cannot hold the
seat, so a hand on the desk during a run joins it, and under Xwayland its
pointer motion moves the desktop's own pointer, since the compositor is
told of it as emulated input. A run on a live desktop is therefore best
left alone while it goes.

## The standards

The picture standards are of a desktop and a backend: the Wayland module
draws its own frames at the buffer's scale and the X module has the window
manager's, so the same test draws two looks on one desktop. They live under
tests/linux_compare by desktop and backend, gnome/wayland, gnome/x11,
plasma/wayland and plasma/x11, each holding <test>.cmp; bin/regress picks
the desktop from XDG_CURRENT_DESKTOP and the backend from the symbols the
test's binary links, and a set that is not there yet is made with --update.
