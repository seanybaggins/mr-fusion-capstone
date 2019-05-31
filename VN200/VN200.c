/***************************************************************************\
 *
 * File:
 * 	VN200.c
 *
 * Description:
 *	Common functionality for VN200, GPS and IMU
 * 
 * Author:
 * 	David Stockhouse
 *
 * Revision 0.1
 * 	Last edited 5/06/2019
 *
 * Revision 0.2
 * 	Last edited 5/30/2019
 * 	Major overhaul unifying GPS and IMU functionality
 *
 ***************************************************************************/

#include "VN200.h"

#include "VN200Packet.h"
#include "VN200_GPS.h"
#include "VN200_IMU.h"
#include "crc.h"

#include "../uart/uart.h"
#include "../buffer/buffer.h"
#include "../logger/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>


/**** Function getTimestamp ****
 *
 * Gets a system timestamp as a double and struct timespec
 *
 * Arguments: 
 * 	ts - Pointer to struct timespec to be used in gettime function
 * 	td - Pointer to double to store processed timestamp
 *
 * Return value:
 *	On success, returns 0
 *	On failure, returns a negative number
 */
int getTimestamp(struct timespec *ts, double *td) {

	int rc;

	if(ts == NULL || td == NULL) {
		return -2;
	}

	// Get system time
	rc = clock_gettime(CLOCK_REALTIME, ts);
	if(rc) {
		return rc;
	}

	// Return (by ref) time as double
	*td = ((double) ts->tv_sec) + ((double) ts->tv_nsec) / 1000000000;

	return 0;

} // getTimestamp(struct timespec *, double *)


/**** Function VN200BaseInit ****
 *
 * Initializes a VN200 IMU/GPS before it is setup for either functionality.
 * Like ADS-B UART initializer, but does not initialize log file
 *
 * Arguments: 
 * 	dev - Pointer to VN200_DEV instance to initialize
 *
 * Return value:
 *	On success, returns 0
 *	On failure, returns a negative number
 */
int VN200BaseInit(VN200_DEV *dev, char *devname, int baud) {

	int i;

	// Exit on error if invalid pointer
	if(dev == NULL) {
		return -1;
	}

	// If device name not given, use default
	if(devname == NULL) {
		dev->fd = UARTInit(VN200_DEVNAME, baud);
	} else {
		dev->fd = UARTInit(devname, baud);
	}

	if(dev->fd < 0) {
		printf("Couldn't initialize VN200 sensor\n");
		return -2;
	}

	// Initialize the input and output buffers
	BufferEmpty(&(dev->inbuf));
	BufferEmpty(&(dev->outbuf));

	// Initialize packet ring buffer
	dev->ringbuf.start = 0;
	dev->ringbuf.end = 0;
	dev->ringbuf.buf = &(dev->inbuf);

	return 0;

} // VN200BaseInit(VN200_DEV *)


/**** Function VN200Poll ****
 *
 * Polls the UART file for an initialized VN200 device and populates inbuf with
 * read data. If it detects the start of a packet with '$' it will also move
 * the data into the next available VN200_PACKET in the ring buffer
 *
 * Arguments: 
 * 	dev - Pointer to VN200_DEV instance to poll
 *
 * Return value:
 *	On success, returns the number of bytes received (may be 0)
 *	On failure, returns a negative number
 */
int VN200Poll(VN200_DEV *dev) {

	int numToRead, numRead, rc, ioctl_status;
	char *startBuf, tempBuf[BYTE_BUFFER_LEN];

	// Exit on error if invalid pointer
	if(dev == NULL) {
		return -1;
	}

	// Ensure length of buffer is long enough to hold more data
	if(dev->inbuf.length >= BYTE_BUFFER_LEN) {
		printf("VN200Poll: Input buffer is full (%d bytes)\n",
				dev->inbuf.length);
		return -2;
	}

	// Check if UART data available
	rc = ioctl(dev->fd, FIONREAD, &ioctl_status);
	if(rc) {
		perror("VN200Poll: ioctl() failed");
		// return -3;
	}
	// printf("%d bytes avail...\n", ioctl_status);

	// Calculate length and pointer to proper position in array
	numToRead = BYTE_BUFFER_LEN - dev->inbuf.length;
	startIndex = dev->inbuf.length;
	startBuf = &(dev->inbuf.buffer[startIndex]);
	// printf("Poll: startBuf is %p\n", startBuf);
	// startBuf[0] = 2;

	// printf("Attempting to read %d bytes from uart device...\n", numToRead);

	// Read without blocking from UART device
	numRead = UARTRead(dev->fd, startBuf, numToRead);
	// numRead = UARTRead(dev->fd, tempBuf, numToRead);
	// printf("\tRead %d\n", numRead);

	// Log newly read data to file
	LogUpdate(&(dev->logFile), startBuf, numRead);
	// LogUpdate(&(dev->logFile), tempBuf, numRead);

	// memcpy(&(dev->inbuf.buffer[dev->inbuf.length]), tempBuf, numRead);

	// Update input buffer endpoint
	dev->inbuf.length += numRead;


	// Update ring buffer endpoint
	VN200PacketRingBufferUpdateEndpoint(&(dev->ringbuf));

	/* Now handled by UpdateEndpoint function
	// Populate most recent packet with data and/or start a new packet
	for(i = 0; i < numRead; i++) {

		// If start of packet, create new packet
		if(startBuf[i] == '$') {

			// Initialize new packet
			rc = VN200PacketRingBufferAddPacket(&(dev->ringbuf));
			if(rc < 1) {
				printf("VN200Parse: Couldn't add packet to ring buffer\n");
				return -1;
			}

		}
		
		// If ring buffer is not empty
		// (there is a partially complete packet)
		if(!VN200RingBufferIsEmpty(&(dev->ringbuf))) {

			// Add character to packet data buffer
			rc = VN200PacketRingBufferAddData(&(dev->ringbuf), startBuf[i]);
			if(rc < 1) {
				return rc;
			}

		} else {

			// Printout incomplete packet data
			printf("%c", startBuf[i]);

		}

	} // for(i < numRead)
	*/

	// Return number successfully read (may be 0)
	return numRead;

} // VN200Poll(VN200_DEV *)


/**** Function VN200Consume ****
 *
 * Consumes bytes in the input buffer
 *
 * Arguments: 
 * 	dev - Pointer to VN200_DEV instance to modify
 * 	num - Number of bytes to consume
 *
 * Return value:
 *	On success, returns number of bytes consumed
 *	On failure, returns a negative number
 */
int VN200Consume(VN200_DEV *dev, int num) {

	// Exit on error if invalid pointer
	if(dev == NULL) {
		return -1;
	}

	// printf("Attempting to consume %d bytes\n", num);
	num = BufferRemove(&(dev->inbuf), num);
	// printf("Consumed %d bytes\n", num);

	return num;

} // VN200Consume(VN200_DEV *, int)


/**** Function VN200FlushInput ****
 *
 * Discards all data recieved through UART
 *
 * Arguments: 
 * 	dev - Pointer to VN200_DEV instance to modify
 *
 * Return value:
 *	On success, returns number of bytes discarded
 *	On failure, returns a negative number
 */
int VN200FlushInput(VN200_DEV *dev) {

	int num, start, i;

	// Exit on error if invalid pointer
	if(dev == NULL) {
		return -1;
	}

	start = dev->inbuf.length;

	// Get all waiting characters from UART
	num = VN200Poll(dev);

	/*
	// Print input before discarding
	printf("Flushed input:\n");
	for(i = start; i < dev->inbuf.length; i++) {
		printf("%c", dev->inbuf.buffer[i]);
	}
	printf("\n");
	*/

	// Clear all characters from input buffer
	num = VN200Consume(dev, num);

	return num;

} // VN200FlushInput(VN200_DEV *)


/**** Function VN200Command ****
 *
 * Writes data to the VN200 output buffer, then flushes the output to UART.
 * Follows serial_cmd.m Matlab function
 *
 * Arguments: 
 * 	dev - Pointer to VN200_DEV instance to modify
 * 	buf - Pointer to data to be written out
 * 	num - Number of bytes to write
 *
 * Return value:
 *	On success, returns number of bytes written
 *	On failure, returns a negative number
 */
int VN200Command(VN200_DEV *dev, char *cmd, int num, int sendChk) {

	char buf[64];
	unsigned char checksum;
	int numWritten, i;

	// Ensure valid pointers
	if(dev == NULL || cmd == NULL) {
		return -1;
	}


	if(sendChk) {

		// Compute and send checksum
		// checksum = calculateChecksum(cmd, num);
		checksum = calculateChecksum(cmd, strlen(cmd));
		numWritten = snprintf(buf, 64, "$%s*%02X\n", cmd, checksum);

	} else {

		// Send XX instead of checksum
		numWritten = snprintf(buf, 64, "$%s*XX\n", cmd);

	} // if(sendChk)

	// Add command string to output buffer
	BufferAddArray(&(dev->outbuf), buf, numWritten);

	/*
	printf("Output buffer contents: \n");
	for(i = 0; i < dev->outbuf.length; i++) {
		printf("%02X", dev->outbuf.buffer[i]);
	}
	printf("\n");
	*/

	// Send output buffer to UART
	numWritten = VN200FlushOutput(dev);

	return numWritten;

} // VN200Command(VN200_DEV &, char *, int, int)


/**** Function VN200FlushOutput ****
 *
 * Writes out data from VN200_DEV struct output buffer to the UART PHY
 *
 * Arguments: 
 * 	dev - Pointer to VN200_DEV instance to modify
 *
 * Return value:
 *	On success, returns number of bytes written
 *	On failure, returns a negative number
 */
int VN200FlushOutput(VN200_DEV *dev) {

	int numWritten, i;

	// Ensure valid pointers
	if(dev == NULL) {
		return -1;
	}

	// Write output buffer to UART
	numWritten = UARTWrite(dev->fd, dev->outbuf.buffer, dev->outbuf.length);

	/*
	printf("Output: \n");
	for(i = 0; i < dev->outbuf.length; i++) {
		printf("%c", dev->outbuf.buffer[i]);
	}
	printf("\n");
	*/

	// Remove the data from the output buffer
	BufferRemove(&(dev->outbuf), numWritten);

	return numWritten;

} // VN200FlushOutput(VN200_DEV &)


/**** Function VN200Destroy ****
 *
 * Cleans up an initialized VN200_DEV instance
 *
 * Arguments: 
 * 	dev - Pointer to VN200_DEV instance to destroy
 *
 * Return value:
 *	On success, returns 0
 *	On failure, returns a negative number
 */
int VN200Destroy(VN200_DEV *dev) {

	if(dev == NULL) {
		return -1;
	}

	// Close UART file
	UARTClose(dev->fd);

	// Close log file
	LogClose(&(dev->logFile));

	return 0;

} // VN200Destroy(VN200_DEV &)


/**** Function VN200Init ****
 *
 * Initializes a VN200 UART device for both GPS and IMU functionality
 *
 * Arguments: 
 * 	dev  - Pointer to VN200_DEV instance to initialize
 * 	fs   - Sampling frequency to initialize the module to
 * 	baud - Baud rate to configure the UART
 * 	mode - Initialization mode (found in VN200.h)
 *
 * Return value:
 *	On success, returns 0
 *	On failure, returns a negative number
 */
int VN200Init(VN200_DEV *dev, int fs, int baud, int mode) {

#define CMD_BUFFER_SIZE 64
	char commandBuf[CMD_BUFFER_SIZE], logBuf[256];
	int commandBufLen, logBufLen;

	char logFileDirName[512];
	int logFileDirNameLength;

	// Exit on error if invalid pointer
	if(dev == NULL) {
		return -1;
	}

	// Ensure valid init mode
	if(!(mode == VN200_INIT_MODE_GPS ||
	     mode == VN200_INIT_MODE_IMU ||
	     mode == VN200_INIT_MODE_BOTH)) {

		printf("VN200Init: Mode 0x%02x not recognized.\n", mode);
		return -2;
	}

	// Ensure valid sample rate (later)



	// Initialize UART for all modes
	dev->baud = baud;
	VN200BaseInit(dev, dev->baud);

	// Initialize log file for raw and parsed data
	// Since multiple log files will be generated for the run, put them in
	// the same directory
	logFileDirNameLength = generateFilename(logFileDirName, 512,
			"../SampleData/VN200", "run", "d");
	LogInit(&(dev->logFile), logFileDirName, "VN200", LOG_FILEEXT_LOG);

	// If GPS enabled, init GPS log file
	if(mode & VN200_INIT_MODE_GPS) {

		// Init csv file
		LogInit(&(dev->logFileGPSParsed), logFileDirName, "VN200_GPS", LOG_FILEEXT_CSV);

		// Write header to CSV data
		logBufLen = snprintf(logBuf, 256, "gpstime,week,gpsfix,numsats,lat,lon,alt,velx,vely,velz,nacc,eacc,vacc,sacc,tacc,timestamp\n");
		LogUpdate(&(dev->logFileGPSParsed), logBuf, logBufLen);

	}
	
	// If IMU enabled, init IMU log file
	if(mode & VN200_INIT_MODE_IMU) {

		// Init csv file
		LogInit(&(dev->logFileIMUParsed), logFileDirName, "VN200_IMU", LOG_FILEEXT_CSV);

		// Write header to CSV data
		logBufLen = snprintf(logBuf, 256, "compx,compy,compz,accelx,accely,accelz,gyrox,gyroy,gyroz,temp,baro,timestamp\n");
		LogUpdate(&(dev->logFileIMUParsed), logBuf, logBufLen);

	}


	/**** Initialize VN200 using commands ****/

	// Request VN200 serial number
	commandBufLen = snprintf(commandBuf, CMD_BUFFER_SIZE, "%s", "VNRRG,03");
	VN200Command(dev, commandBuf, commandBufLen, 1);
	usleep(100000);

	// Disable asynchronous output
	commandBufLen = snprintf(commandBuf, CMD_BUFFER_SIZE, "%s", "VNWRG,06,0");
	VN200Command(dev, commandBuf, commandBufLen, 1);
	usleep(100000);

	// Set IMU sampling frequency
	dev->fs = fs;
	commandBufLen = snprintf(commandBuf, CMD_BUFFER_SIZE, "%s%d", "VNWRG,07,", dev->fs);
	VN200Command(dev, commandBuf, commandBufLen, 1);
	usleep(100000);


	if(mode == VN200_INIT_MODE_GPS) {

		// Enable asynchronous GPS data output
		commandBufLen = snprintf(commandBuf, CMD_BUFFER_SIZE, "%s", "VNWRG,06,20");

	} else if(mode == VN200_INIT_MODE_IMU) {

		// Enable asynchronous IMU data output
		commandBufLen = snprintf(commandBuf, CMD_BUFFER_SIZE, "%s", "VNWRG,06,19");

	} else {

		// Enable both GPS and IMU output
		commandBufLen = snprintf(commandBuf, CMD_BUFFER_SIZE, "%s", "VNWRG,06,248");

	}


	// Send mode command to UART
	VN200Command(dev, commandBuf, commandBufLen, 1);
	usleep(100000);

	// Clear input buffer (temporary)
	VN200FlushInput(dev);

	return 0;

} // VN200Init(VN200_DEV *, int)


/**** Function VN200Parse ****
 *
 * Parses data from VN200 input buffer and determine packet type. Will parse as
 * many packets as are available in the buffer
 *
 * Arguments: 
 * 	dev  - Pointer to VN200_DEV instance to parse from
 *
 * Return value:
 *	On success, returns number of bytes parsed from buffer
 *	On failure, returns a negative number
 */
int VN200Parse(VN200_DEV *dev) {

	// Make extra sure there is enough room in the buffer
#define PACKET_BUF_SIZE 1024
	char currentPacket[PACKET_BUF_SIZE], logBuf[512], packetID[16];

	unsigned char chkOld, chkNew;
	int packetIDLength, packetIndex, logBufLen, numParsed = 0, i, rc;
	struct timespec timestamp_ts;

	VN200_PACKET *packet; // Pointer to the current packet being parsed
	VN200_PACKET_RING_BUFFER *ringbuf;


	// Exit on error if invalid pointer
	if(dev == NULL) {
		return -1;
	}

	// Make local ringbuf pointer
	ringbuf = &(dev->ringbuf);

	// Loop through all packets in ring buffer
	for(packetIndex = ringbuf->start; packetIndex != ringbuf->end;
			packetIndex = (packetIndex + 1) % VN200_PACKET_RING_BUFFER_SIZE) {

		// Set up pointer to current packet
		packet = &(ringbuf->packets[packetIndex]);

		// If packet is incomplete or already parsed, do nothing and return
		if(VN200PacketIsIncomplete(packet)) {
			return numParsed;
		}

		// Only parse if hasn't already been parsed
		if(!(packet->isParsed)) {

			VN200PacketParse(ringbuf, packetIndex);

		} // if packet not already parsed

	} // for packets in ring buffer

	return packetEnd + 3;

} // VN200Parse(VN200_DEV *)

