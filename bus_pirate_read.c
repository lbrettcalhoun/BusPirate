/* 
This program uses the Bus Pirate to read data from an 24LC08B EEPROM.  This program uses Canonical input (default).  Canonical input
offers no advantage for this program; it's just the default.
*/ 

#include <stdio.h>
#include <unistd.h>
#include <termios.h>		// POSIX terminal control definitions
#include <errno.h>		// Error number definitions
#include <fcntl.h>		// File control definitions
#include <strings.h>
#include <stdlib.h>
#include <string.h>

#define BUFFERSIZE 1024
#define DEBUG
#define BBEN "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"  // Send 20 null chars (\0) to ENABLE bitbang or binary mode
#define I2CEN "\x2"			      	         // Send a STX char (\x2) to ENABLE I2C mode
#define PPEN "\x4C"				         // Send a L char (\x4C) to enable power and pullup resistors
#define STARTWRITE "\x2"				 // Send a start bit
#define STOPWRITE "\x3"					 // Send a stop bit
#define BULKWRITE1 "\x11"				 // Send the first bulk write command
#define BULKWRITE2 "\x10"				 // Send the second bulk write command
#define READWRITE "\x4"				         // Send a read byte command
#define BBDIS "\xF"					 // Send a SI char (\xF) to DISABLE bitbang or binary mode
#define I2CDIS "\0"					 // Send a null char (\0) to DISABLE I2C mode
#define DEVWRITEADDR "\xA0"				 // Send the device write address ... see 24LC08B data sheet
#define DEVREADADDR "\xA1"				 // Send the device read address ... see 24LC08B data sheet
#define NACKWRITE "\x7"					 // Send a NACK

int main (void) {

  // Define variables
  int fd, result, i, readaddress;
  struct termios portopts;
  char BPbuffer[BUFFERSIZE];
  char outputbuffer[BUFFERSIZE];
  char addressbuffer[1];

  result = 0;
  readaddress = 0;
  addressbuffer[0] = readaddress;

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
  //
  // Note:  Hmmm ... even though all flags are cleared, I'm still having problems reading the input from serial
  // device when the Bus Pirate is connected to USB port on docking station.  Add a usleep to each read to introduce
  // a delay before we read.
  result =  fcntl(fd, F_SETFL, 0);

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

  for (i=0; i<(sizeof(outputbuffer)); i++) {

    // Put the Bus Pirate in "bitbang" or binary mode.  Send the ASCII "null" (\0 or \x0) 20 times to get into binary
    // mode.  The Bus Pirate will answer with "BBIO1".
    result = write(fd, BBEN, 20); 
     
    if (result == -1) {
      perror("Cannot send bitbang command to Bus Pirate - ");
      close (fd);
      exit(2);
    } 
  
    usleep (10000);
    result = read(fd, &BPbuffer, 5);
  
    if (result <= 0 ) {
      perror("Could not read bitbang output from Bus Pirate - ");
      close (fd);
      exit (3);
    }

    if (strncmp("BBIO1", BPbuffer, 5) != 0) {
      puts ("Could not enable binary mode on Bus Pirate");
      close (fd);
      exit (4);
    }
  
    // Put the Bus Pirate in "I2C"  mode.  Bus Pirate will answer with "I2C1".
    result = write(fd, I2CEN, 1);
   
    if (result == -1) {
      perror("Cannot send I2C command to Bus Pirate - ");
      close (fd);
      exit(2);
    } 

    usleep (10000);   
    result = read(fd, &BPbuffer, 4);
  
    if (result <= 0 ) {
      perror("Could not read I2C output from Bus Pirate - ");
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (3);
    }
  
    if (strncmp("I2C1", BPbuffer, 4) != 0) {
      puts("Could not enable I2C mode on Bus Pirate");
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (4); 
    }
  
    // Configure the Bus Pirate peripherals (W:  Power on, P:  Pullups on):  01001100 ... 0x4C ... see I2C (binary) - DP for details
    // Bus Pirate will return 0x1 when peripherals on enabled
    result = write(fd, PPEN, 1);
   
    if (result == -1) {
      perror("Cannot send peripherals command to Bus Pirate - ");
      close (fd);
      exit(2);
    } 

    usleep (10000);   
    result = read(fd, &BPbuffer, 1);
  
    if (result <= 0 ) {
      perror("Could not read peripherals output from Bus Pirate - ");
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (3);
    }
  
    if (1 != BPbuffer[0]) {
      puts("Could not enable peripherals mode on Bus Pirate");
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (4); 
    }
  
    // Read the data from the EEPROM.  This is an 24LC08B.  It uses following addresses for read:
    // 0xA1
    // Here is the read instruction:  [10100000 0 [10100001 r].
    // 
    // So here's how this works:
    //  - Send the start bit
    //  - Send the first bulk write command which specifies the number of bytes to write:  2 ... device write address, address
    //  - Send the device write address
    //  - Send the read address
    //  - Send another start bit
    //  - Send another bulk write command which specifies the number of bytes to write:  1 ... device read address
    //  - Send the device read address
    //  - Send the read command to read a byte
    //  - Send a NACK
    //  - Send a stop bit
    //  - Repeat

    // Send I2C start bit
    result = write (fd, STARTWRITE, 1);
  
    if (result == -1) {
      perror ("Could not send start bit to EEPROM - ");
      close (fd); 
      exit (2);
    }

    usleep (10000);  
    result = read (fd, &BPbuffer, 1);
  
    if (result != 1) { 
      perror ("Could not read I2C response - start bit - ");
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (3); 
    }
  
    if (1 != BPbuffer[0]) {
      puts("Start bit error on Bus Pirate");
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (4); 
    }
  
    // Send first I2C bulk write command 17 (10001) ... 16 (10000) for the bulk write command + 1 (1) to write 2 bytes ... yes ... 1
    // is 2 ... see documentation.  Those 2 bytes are:  device address, write address (changes each time through loop)
    result = write (fd, BULKWRITE1, 1);
  
    if (result == -1) {
      perror ("Could not send bulk write to EEPROM - ");
      close (fd); 
      exit (2);
    }
 
    usleep (10000); 
    result = read (fd, &BPbuffer, 1);
  
    if (result != 1) { 
      perror ("Could not read I2C response - bulk write command - ");
      result = write (fd, STOPWRITE, 1);
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (3); 
    }
  
    if (1 != BPbuffer[0]) {
      puts ("Bulk write error on Bus Pirate");
      result = write (fd, STOPWRITE, 1);
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (4); 
    }
    
    // Send the device write address
    result = write (fd, DEVWRITEADDR, 1);
  
    if (result == -1) {
      perror ("Could not send device address to EEPROM - ");
      close (fd); 
      exit (2);
    }
 
    usleep (10000);  
    result = read (fd, &BPbuffer, 1);
  
    if (result != 1) { 
      perror ("Could not read I2C response - device address write command - ");
      result = write (fd, STOPWRITE, 1);
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (3); 
    }
  
    if (1 == BPbuffer[0]) {    // Unlike our previous responses ... a 1 in the buffer means a NACK ... something went wrong
      puts ("Device address write error on Bus Pirate - NACK");
      result = write (fd, STOPWRITE, 1);
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (4); 
    }

    // Send the read address
    result = write (fd, &addressbuffer, 1);
   
    readaddress++; 
    addressbuffer[0] = readaddress;

    if (result == -1) {
      perror ("Could not send read address to EEPROM - ");
      close (fd); 
      exit (2);
    }
  
    usleep (10000);
    result = read (fd, &BPbuffer, 1);
  
    if (result != 1) { 
      perror ("Could not read I2C response - read address write command - ");
      result = write (fd, STOPWRITE, 1);
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (3); 
    }
  
    if (1 == BPbuffer[0]) {
      puts("Read address write error on Bus Pirate");    // Same here ... 1 means a NACK
      result = write (fd, STOPWRITE, 1);
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (4); 
    }

    // Send another I2C start bit
    result = write (fd, STARTWRITE, 1);
  
    if (result == -1) {
      perror ("Could not send start bit to EEPROM - ");
      close (fd); 
      exit (2);
    }

    usleep (10000);  
    result = read (fd, &BPbuffer, 1);
  
    if (result != 1) { 
      perror ("Could not read I2C response - start bit - ");
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (3); 
    }
  
    if (1 != BPbuffer[0]) {
      puts("Start bit error on Bus Pirate");
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (4); 
    }
  
    // Send second I2C bulk write command 16 (10000) ... 16 (10000) for the bulk write command + 0 (0) to write 1 bytes ... yes ... 0
    // is 1 ... see documentation.  The 1 byte is:  device read address
    result = write (fd, BULKWRITE2, 1);
  
    if (result == -1) {
      perror ("Could not send bulk write to EEPROM - ");
      close (fd); 
      exit (2);
    }
 
    usleep (10000); 
    result = read (fd, &BPbuffer, 1);
  
    if (result != 1) { 
      perror ("Could not read I2C response - bulk write command - ");
      result = write (fd, STOPWRITE, 1);
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (3); 
    }
  
    if (1 != BPbuffer[0]) {
      puts ("Bulk write error on Bus Pirate");
      result = write (fd, STOPWRITE, 1);
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (4); 
    }

    // Send the device read address
    result = write (fd, DEVREADADDR, 1);
  
    if (result == -1) {
      perror ("Could not send device address to EEPROM - ");
      close (fd); 
      exit (2);
    }
 
    usleep (10000);  
    result = read (fd, &BPbuffer, 1);
  
    if (result != 1) { 
      perror ("Could not read I2C response - device address write command - ");
      result = write (fd, STOPWRITE, 1);
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (3); 
    }
  
    if (1 == BPbuffer[0]) {    // Unlike our previous responses ... a 1 in the buffer means a NACK ... something went wrong
      puts ("Device address write error on Bus Pirate - NACK");
      result = write (fd, STOPWRITE, 1);
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (4); 
    }

    // Send read command ... copy the byte into the output buffer.  Also, check for our "EOD" marker (0xA ... new line) and 
    // break out of the loop when you find it
    result = write (fd, READWRITE, 1);
  
    if (result == -1) {
      perror ("Could not send read to EEPROM - ");
      close (fd); 
      exit (2);
    }

    usleep (10000);  
    result = read (fd, &BPbuffer, 1);
    outputbuffer[i] = BPbuffer[0];
  
    if (result != 1) { 
      perror ("Could not read I2C response - read - ");
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (3); 
    }

    if (10 == BPbuffer[0]) {
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      break;
    }

    // Send NACK 
    result = write (fd, NACKWRITE, 1);
  
    if (result == -1) {
      perror ("Could not send NACK to EEPROM - ");
      close (fd); 
      exit (2);
    }

    usleep (10000);  
    result = read (fd, &BPbuffer, 1);
 
    if (result != 1) { 
      perror ("Could not read I2C response - NACK - ");
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (3); 
    }
  
    if (1 != BPbuffer[0]) {
      puts("NACK error on Bus Pirate");
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (4); 
    }

    // Send I2C stop bit
    result = write (fd, STOPWRITE, 1);
  
    if (result == -1) {
      perror ("Could not send stop bit to EEPROM - ");
      close (fd); 
      exit (2);
    }
  
    usleep (10000);
    result = read (fd, &BPbuffer, 1);
  
    if (result != 1) { 
      perror ("Could not read I2C response - stop bit - ");
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (3);
    }
  
    if (1 != BPbuffer[0]) {
      puts("Stop bit write error on Bus Pirate");
      result = write (fd, I2CDIS, 1);
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (4); 
    }
 
    // Disable I2C mode ... put the Bus Pirate back into bitbang mode
    result = write(fd, I2CDIS, 1);
   
    if (result == -1) {
      perror("Cannot send bitbang command to Bus Pirate - ");
      close (fd);
      exit(2);
    } 

    usleep (10000);
    result = read(fd, &BPbuffer, 5);

    if (result <= 0 ) {
      perror("Could not read bitbang output from Bus Pirate - ");
      close (fd);
      exit (3);
    }

    if (strncmp("BBIO1", BPbuffer, 5) != 0) {
      puts("Could not disable I2C mode on Bus Pirate");
      result = write (fd, BBDIS, 1);
      close (fd);
      exit (4); 
    }

    // Disable binary mode ... put the Bus Pirate back into user mode ... aka reset
    result = write(fd, BBDIS, 1);
   
    if (result == -1) {
      perror("Cannot send reset command to Bus Pirate - ");
      close (fd);
      exit(2);
    } 

    usleep (10000);
    result = read(fd, &BPbuffer, 1);

    if (result <= 0 ) {
      perror("Could not read reset output from Bus Pirate - ");
      close (fd);
      exit (3);
    }
    if (1 != BPbuffer[0]) {
      puts("Could not reset Bus Pirate");
      close (fd);
      exit (4); 
    }

    // Once back in user mode, the Bus Pirate will print hardware and firmware version ... read this output before
    // trying to enter binary mode again.  And yes, it takes 2 reads to read this output.
    usleep (10000);
    result = read(fd, &BPbuffer, 132);
    usleep (10000);
    result = read(fd, &BPbuffer, 132);
  }

  // Close the serial port
  close (fd);

  // Print the output buffer on the screen
  printf ("%s\n", outputbuffer);

}
