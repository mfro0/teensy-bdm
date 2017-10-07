#define VERSION_SW  0x04  /* make sure to change the USB description (StringDesc2) when changing the SW revision !! */
#define VERSION_HW  0x2B
#define VERSION     (VERSION_HW+256*VERSION_SW)  /* intel endianism */

/* SW Changes:

0.1   10/03/2006    Initial coding
0.2   30/05/2006    Added USB suspend functionality and removed debugging code from main.c
0.3   21/06/2006    Improved HALT and RESET command functionality
0.4   22/06/2006    Added clean-up in case any command fails to clear the BDM of the offending command
                    Fixed block commands - not all failed transfers were reported  

*/

/* Known bugs:

1. device not functional after computer hibernation

When the device is attached and the computer is hibernated
and then woken up again, the computer does not seem to want to talk to the device
anymore. I do not know what to do about this. Other devices (like my mouse) seem to 
survive the same thing OK, so it must be the device, but what is it? Enabling/disabling
the device in device manager brings it into stop and wakes it up again as expected. The 
computer cuts power to the device during hibernation, what is it expecting??

*/
