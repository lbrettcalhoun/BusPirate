/* 
L. Brett Calhoun

This program uses the Bus Pirate to write user specified input from the terminal to
an 24LC08B EEPROM.  This program uses Canonical input (default).  Canonical input
offers no advantage for this program; it's just the default.

Version 2.0:  Use bulk write to send data 8 bytes at a time.  Use a character array to
hold the BB enable, I2C enable, power/pullup, start bit, bulk write command, data, and stop bit.
Yes ... everything for a complete write cycle in the array.  This will write in blocks of
8 bytes.

The reason we have to write in blocks of 8 bytes is because of the 24LC08B page write limitation.
Page writes must be performed within single pages (16 bytes).  You cannot perform write operations
across multiple pages.  Since the Bus Pirate bulk write only supports up to 14 data bytes, I chose
to artificially limit the bulk writes to 8 bytes (1/2 of a page).  

Creds:
I owe a debt of gratitude to James Stephenson.  I used his I2CEEPROMWIN.c to understand how to
to prepare and populate the write buffer and parse the response (among other things).  Thanks James!
I also owe a debt of gratitude to Michael Sweet for his Serial Programming Guide for POSIX Operating Systems.
And finally, thanks to the excellent tutorials on the Bus Pirate web site.
*/ 

#include <stdio.h>
#include <unistd.h>
#include <termios.h>		// POSIX terminal control definitions
#include <errno.h>		// Error number definitions
#include <fcntl.h>		// File control definitions
#include <strings.h>
#include <stdlib.h>
#include <string.h>

#define BUFFERSIZE 256
#define DEBUG

int main (void) {

  // Define variables
  int fd, result, i, j, k, writeaddress, inputbuffercount, numloops;
  struct termios portopts;
  char BPbuffer[BUFFERSIZE];
  char inputbuffer[BUFFERSIZE];
  char addressbuffer[1];
  char writebuffer[38]; 		// This is our main working buffer ... all the Bus Pirate commands go here.

  result = 0;
  writeaddress = 0;
  inputbuffercount = 0;
  numloops = 0;
  bzero (BPbuffer, sizeof (BPbuffer));
  bzero (inputbuffer, sizeof (inputbuffer));
  bzero (writebuffer, sizeof (writebuffer));
  bzero (addressbuffer, sizeof (addressbuffer)); 

  // Populate the writebuffer with our static values (such as the 20 null bytes for bitbang enable) and dynamic values
  // (such as the data bytes).  Later on (inside the loop), we'll overwrite the dynamic values.
  //  - enable bitbang:  20 null bytes (\0) ... since we used bzero above we don't need to populate these values!
  //  - enable I2C:  \2
  //  - enable power and pullup:  \x4C
  //  - start bit: \2
  //  - 8 byte bulk write command:  \x19
  //  - device write address:  \xA0
  //  - write address:  dynamic ... will change each time through loop ... use 0xAA for placeholder
  //  - data bytes:  dynamic ... will change each time through loop ... use 0xBB for placeholders
  //  - stop bit:  \3 
  //  - disable I2C:  \0
  //  - disable bitbang:  \xF
  //  - our special flag to help spot end of buffer during debugging:  \xEE
  writebuffer[20] = '\2';
  writebuffer[21] = '\x4C';
  writebuffer[22] = '\2';
  writebuffer[23] = '\x1F';
  writebuffer[24] = '\xA0';
  writebuffer[25] = '\xAA';

  for (i = 26; i < 34; i++) {
    writebuffer[i] = '\xBB';
  }

  writebuffer[34] = '\3';
  writebuffer[35] = '\0';
  writebuffer[36] = '\xF';
  writebuffer[37] = '\xEE';		// Special flag ... look for this flag in gdb:  x/38xb writebuffer

  // Get input from terminal
  printf ("Enter to end (%d chars max)> ", BUFFERSIZE);
  fgets (inputbuffer, BUFFERSIZE, stdin);

  // Open the serial port.  The Bus Pirate will be attached as /dev/ttyUSB0.  Open the port with R/W, no delay and "no controlling
  // terminal" options.  The latter option will keep unwanted keyboard abort signals from affecting this program.
  fd = open ("/dev/ttyUSB0",O_RDWR | O_NOCTTY | O_NDELAY);

  if (result == -1) {
    perror ("Unable to open /dev/ttyUSB0 - ");
    exit(1);
  }

  // Change the serial device (fd) flags ... clear all flags ... a brute force way to clear the O_NDELAY flag so
  // that the program will block and wait for input from serial device before executing read.  Otherwise, the Bus
  // Pirate will not be able to process the write and send input to read fast enough.
  // result =  fcntl(fd, F_SETFL, 0);

  if (result == -1) {
    perror("Cannot set serial device flag - ");
    close (fd);
    exit(7);
  } 

  // Set serial port options for the Bus Pirate.  Use the termios structure (available in termios.h) and it's associated
  // constants to set the options.  Before you can set the options, you have to initialize your portopts structure by
  // getting the existing port options.  The options don't take effect until you call tcsetattr.  Use the TCSANOW constant
  // to set the settings IMMEDIATELY. Unlike speed, there is no function to set the character size and parity.  Just use
  // bit masking to set these settings (8N1).
  tcgetattr (fd, &portopts);
  memset (&portopts, 0, sizeof(struct termios)); // Initialize all elements of the portopts structure to 0
  cfsetspeed (&portopts,B115200);
  tcsetattr (fd, TCSANOW, &portopts);
  portopts.c_cflag &= ~PARENB;	// Disable parity bit ... note the use of bitwise NOT (~) in front of constant
  portopts.c_cflag &= ~CSTOPB;  // Disable 2 stop bits ... use 1 stop instead ... note the use of bitwise NOT as above
  portopts.c_cflag &= ~CSIZE;   // Clear the existing character size bits ... again ... note the use of bitwise NOT
  portopts.c_cflag &= CS8;      // Set the mask bits for 8 characters

  // Figure out how many times we have to loop through the input buffer to process all the data.  We're writing in
  // blocks of 8 bytes ... so loop for every 8 bytes in the input buffer.  Also, we have to account for special cases:  
  // 1) when the input buffer is less than 8 characters (including new line) and 2) when the input buffer
  // has less than 8 characters in the last loop.
  numloops = strlen (inputbuffer) / 8;

  if (numloops == 0) {
    numloops = 1;
  }
  else if (strlen (inputbuffer) % 8 > 0) {
    numloops = numloops++;
  }

  // Now do your main loop ... loop once for every 8 bytes
  for (i = 0; i < numloops; i++) {

    // Populate the write buffer with dynamic values for write address
    writebuffer[25] = writeaddress;
  
    // Populate the write buffer with dynamic values for data.  Loop through the input buffer 8 bytes at a 
    // time ... test for the new line character for the end of data.  When you get out of the loop, j will be
    // equal to the number of data bytes.  We'll use j later on to patch our write buffer and parse the 
    // responses.
    for (j = 0; j < 8; j++) {
      writebuffer[26 + j] = inputbuffer[inputbuffercount];
      inputbuffercount++;
      if (inputbuffer[j] == 10) {
        j++;
        break;
      }
    }
    
    // Patch the write buffer with the new bulk write command, stop bit, disable I2C and disable bitbang bytes
    writebuffer[23] = 16 + 1 + j;	// The new bulk write command: 16 for bulk write + 1 for device address and write address
					// + j for the number of data bytes.  Yes ... 1 for device address and write address.
    writebuffer[26 + j] = '\3';		// Stop bit
    writebuffer[27 + j] = '\0';		// Disable I2C
    writebuffer[28 + j] = '\xF';    	// Disable bitbang
    writebuffer[29 + j] = '\xEE'; 	// Our special EOB for debugging

    // Send the entire write buffer ... you'll always send at least 29 bytes + the number of data bytes (j)
    result = write(fd, writebuffer, 29 + j);
     
    if (result == -1) {
      perror("Cannot send write buffer to Bus Pirate - ");
      close (fd);
      exit(2);
    } 
 
    // Now read the input from Bus Pirate and parse to determine status of write.  You'll always read at least 21 bytes
    // + the number of data bytes (j)
    // Number of response bytes from Bus Pirate
    // Enable bitbang:		5
    // Enable I2C:		4
    // Power and Pullup:	1
    // Start:			1
    // 8 byte bulk write:	1
    // Device address:		1
    // Write address:		1
    // Number of data bytes:	j + 1
    // Stop bit:		1
    // Disable I2C:		5
    // Disable bitbang:		1
    // Total:			21 + j + 1
    usleep (10000);
    result = read(fd, &BPbuffer, 21 + j + 1);
  
    if (result <= 0 ) {
      perror("Could not read output from Bus Pirate - ");
      close (fd);
      exit (3);
    }

    if (strncmp("BBIO1", BPbuffer, 5) != 0) {
      puts ("Could not enable binary mode on Bus Pirate");
      close (fd);
      exit (4);
    }
    if (strncmp("BBIO1I2C1", BPbuffer, 9) != 0) {
      puts ("Could not enable I2C mode on Bus Pirate");
      close (fd);
      exit (4);
    }
    if (BPbuffer[9] != 1) {
      puts ("Could not enable power and pullup  on Bus Pirate");
      close (fd);
      exit (4);
    }
    if (BPbuffer[10] != 1) {
      puts ("Start bit error on Bus Pirate");
      close (fd);
      exit (4);
    }
    if (BPbuffer[11] != 1) {
      puts ("Bulk write command error on Bus Pirate");
      close (fd);
      exit (4);
    } 
    if (BPbuffer[12] != 0) {
      puts ("Did not receive ACK for write device address from Bus Pirate");
      close (fd);
      exit (4);
    } 
    if (BPbuffer[13] != 0) {
      puts ("Did not receive ACK for write address from Bus Pirate");
      close (fd);
      exit (4);
    } 
    for (k = 0; k < j; k++) {  		

      if (BPbuffer[14 + k] != 0) {
        puts ("Did not recieve ACK for data byte from Bus Pirate");
        close (fd);
        exit (4);
      }
    }
    if (BPbuffer[14 + k] != 1) {		// Don't forget that k will be incremented to 8 when it drops out of the loop
      puts ("Stop bit error on Bus Pirate");
      close (fd);
      exit (4);
    }
    if ((BPbuffer[15 + k] != 'B') && (BPbuffer[16 + k] != 'B') && (BPbuffer[17 + k] != 'I') && (BPbuffer[18 + k] != 'O')) {
      puts ("Could not disable binary mode on Bus Pirate");
      close (fd);
      exit (4);
    }
    if (BPbuffer[20 + k] != 1) {
      puts ("Could not reset Bus Pirate to user mode");
      close (fd);
      exit (4);
    }
    
    // Once back in user mode, the Bus Pirate will print hardware and firmware version ... read this output before
    // trying to enter binary mode again.  And yes, it takes 2 reads to read this output.
    usleep (10000);
    result = read(fd, &BPbuffer, 132);
    usleep (10000);
    result = read(fd, &BPbuffer, 132);
    
    // Increment writeaddress
    writeaddress = writeaddress + j;
  }

  // Close the serial port
  close (fd);
}
