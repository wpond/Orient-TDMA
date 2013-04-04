
4th Year Project Submission
Software Engineering

Developing a Time Division Multiple Access
Based Protocol for Efficient Data Recovery in
Harsh Environments Through On Demand
Dynamic Allocation of Bandwidth

William Pond
0818057

=====
PROJECT OUTLINE

/
The main project code which creates the firmware is located in the root directory. A GCC cross compilation environment for ARM must be available to build the project using the makefile.

/basestation
	The Python command line base station. For the Python connection the pySerial library must be installed.
	
	/basestation/tests
		The series of tests performed on the nodes. Each folder includes the raw results produced.

	/basestation/tests/experimental
		The C based implementation of the base station used for testing. Commented input files and their raw counter parts are included. postproc.py performs post processing aggregation on the results.

/results
	The set of results analysed. Analysis was performed on Google Drive and this contains the Excel versions of the spreadsheets including all generated graphs.

/docs
	Documents used during development. These include the pins configured for the TDMA and an initial outline of the radio stack

=====
INCLUDED LIBRARIES

/efm32lib
/efm32usb
/CMSIS
	Energy micro supplied libraries (see files for license).

/basestation/tests/experimental/rs232.{c,h}
	RS 232 (serial) C library, supplied under GPL (see files for license).

/usb.c
	Energy micro USB library, with modifications as part of the project (see file for license).