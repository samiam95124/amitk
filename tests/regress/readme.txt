This directory is the run sandbox for bin/regress. services_test runs
here, and lists ../testdir, whose contents are fixed. The lib
symlink lets the test binaries find the Petit-Ami shared libraries, which
are loaded relative to the current directory.
