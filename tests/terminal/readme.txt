This directory is the run sandbox for the terminal test under bin/regress.
terminal_test runs here, in an xterm on a private X server, or on Windows in
a console of its own, where the configuration file turns the joysticks off
so that the standard is of no machine in particular, and where the file the
writethrough test makes lands out of the way. The lib symlink lets the test
binary find the Petit-Ami shared libraries, which are loaded relative to the
current directory.
