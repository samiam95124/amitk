@echo off
rem A console of its own for a terminal test under bin/regress, as the xterm on
rem a private X server is on Linux: %1 columns, %2 lines, %3 the directory to
rem run in, %4 the program, %5 the listing it captures to, %6 where its
rem standard error goes. Waits for the test to end.
start /wait /min "" cmd /c "mode con cols=%1 lines=%2 & cd /d %3 & %4 auto %5 2> %6"
