/****************************************************************************\
 *
 * File:
 * 	uarttest.c
 *
 * Description:
 * 	Tests the pingUSB UART functionality
 *
 * Author:
 * 	David Stockhouse
 *
 * Revision 0.1
 * 	Last edited 2/13/2019
 *
\***************************************************************************/

#include "buffer.h"
#include "uart.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>


/**** Function main ****
 *
 * Test of the UART module functionality
 *
 * Arguments: None
 *
 * Return value:
 * 	Returns 0 on success
 * 	Returns a negative number on failure
 */
int main(void) {

	int i, rc, numRead, col = 0;

	USB_RECV dev;

	rc = pingUSBInit(&dev);
	if(rc) {
		printf("Couldn't intialize device: %d\n", rc);
		return rc;
	}

	printf("Initializing receiver... %d\n", rc);
	printf("Entering polling loop (ctrl-c to exit)\n\n\t");

	while(1) {

		numRead = pingUSBPoll(&dev);

		// Print chars received in hex
		for(i = 0; i < dev.inbuf.length; i++) {
			printf("%02x", dev.inbuf.buffer[i]);
			col++;
			if(col % 2 == 0) {
				printf(" ");
			}
			if(col % 8 == 0) {
				printf(" ");
			}
			if(col % 16 == 0) {
				printf("\t");
			}
		}
		// pingUSBConsume(&dev, dev.inbuf.length);

		BufferRemove(&(dev.inbuf), dev.inbuf.length);

		usleep(10000);
	}

	printf("\n\nTest complete\n\n");

	pingUSBDestroy(&dev);

	// Success
	return 0;

} // main()

