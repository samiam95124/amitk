! nano.adp - Adapter file for GNU nano text editor
!
! Maps Ami terminal events to nano's control key sequences.
! Nano uses Emacs-style control keys for navigation and editing.

keyequ

    ! navigation
    etup       '^p'       ! previous line
    etdown     '^n'       ! next line
    etleft     '^b'       ! backward one character
    etright    '^f'       ! forward one character
    ethomel    '^a'       ! beginning of line
    etendl     '^e'       ! end of line
    etpagu     '^y'       ! page up
    etpagd     '^v'       ! page down
    ethome     '\x01'     ! beginning of file (same as ^A for now)

    ! editing
    etenter    '\n'       ! enter/newline (nano inserts on LF, not CR)
    ettab      '\t'       ! tab
    etdelcb    '\x08'     ! backspace
    etdelcf    '^d'       ! delete forward
    etdel      '^k'       ! cut line (delete block)
    etinsert   '^u'       ! uncut/paste (insert)
    etcan      '^c'       ! cancel / show cursor position

    ! function keys map to nano commands
    etfun1     '^g'       ! F1 = help
    etfun2     '^o'       ! F2 = write out (save)
    etfun3     '^r'       ! F3 = read file (open/insert)
    etfun4     '^w'       ! F4 = where is (search)
    etfun5     '^\'       ! F5 = replace
    etfun6     '^j'       ! F6 = justify
    etfun7     '^t'       ! F7 = spell check

    ! program control
    etterm     '^x'       ! terminate = exit nano

keyend

! Menu bar for graphics-mode builds (ami_menu)
! These appear in the window frame menu bar and generate the
! corresponding nano control key sequence when clicked.

menu

    'File' submenu
        'Read File  F3'  '^r'
        'Save       F2'  '^o'
        bar
        'Exit       ^X'  '^x'
    submenuend

    'Edit' submenu
        'Cut Line   ^K'  '^k'
        'Paste      ^U'  '^u'
        bar
        'Search     F4'  '^w'
        'Replace    F5'  '^\'
        bar
        'Go To Line ^_'  '^_'
    submenuend

    'Help' submenu
        'Help       F1'  '^g'
        'Position   ^C'  '^c'
    submenuend

menuend
