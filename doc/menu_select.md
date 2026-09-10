# Menu select mode

Menu select mode allows keyboard only selection of menu elements. It starts
with the menu select key, and can be exited with the cancel key. What it does is
select an item from the menu and highlight that, usually in reverse video.
Left and right arrows move the select to different menu items, stopping at
either the first menu item or the last. The menu item is activated by return.
If the menu has an immediate action, that is executed, otherwise if it is a
submenu, that is displayed. A down or right arrow also can reveal a submenu
(down for the top menu, right for a submenu in a list). Thus the full menu tree
can be navigated or items selected.

The menu mode is exited when either a cancel or a menu selection is done. The
menu mode is handled entirely by the windowing code in Ami. The net effect is
as if the mouse was used to select a menu item.
## As implemented

The menu select key is Alt-F12, which produces the display menu event,
ami_etmenu. It is the one key of its kind that nothing else claims:
GNOME Terminal takes F10 and F11, Petit-Ami's own key table has F12 as
its tenth function key, and the desktops take Alt-F1 through Alt-F6
between them. On Wayland and X11, Alt pressed and released by itself is
the same key, as Windows has it; Alt held while another key is pressed
is a modifier as before. A window without a menu is handed the event
and may do as it likes with it.

The highlight is the pressed look of a menu entry on the desktops, and
reverse video on a terminal. Left at the top of a pulldown closes it and
comes back out to the bar entry; Left in a submenu comes back out to
the entry that opened it. A click anywhere ends the mode and then does
what a click does. Cancel also closes a menu the mouse opened, so
Escape is the way out of a menu however it was opened. The mode lives in the Wayland and X11 graphics
modules and in the terminal window manager; the framebuffer's menus are
the portable window manager's and do not have it yet.
