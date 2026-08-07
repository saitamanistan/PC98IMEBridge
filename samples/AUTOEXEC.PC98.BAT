@ECHO OFF
REM FreeDOS(98) AUTOEXEC example for the PC-98 TSR client.
REM Adjust the PATH entries to match the guest installation.
PATH=Z:\FDOS\BIN;Z:\OPT\BIN;Z:\NP2TOOLS;Z:\VZ;Z:\FD;Z:

REM IME98.CFG must be in the current directory (Z:\ in this example).
Z:
REM Install the resident client once. A duplicate launch is rejected safely.
IF EXIST Z:\IME98TSR.COM Z:\IME98TSR.COM

REM IME98.COM is a foreground diagnostic and is not needed for normal use.
REM IF EXIST Z:\IME98.COM Z:\IME98.COM /VERIFY
