#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "socal.h"
#include "hps.h"
#include "hps_soc_system.h"

#define KEY_BASE              0xFF200050
#define VIDEO_IN_BASE         0xFF203060
#define FPGA_ONCHIP_BASE      0xC8000000

short input[240][320];		// [Rows][Cols]
short tempflip[240][320];	// array for imageprocessing
int count = 0;				// Keeps track of image #
char timetext[64];			// Character buffer for timestamp
char counttext[64];			// Character buffer for count
char bitarr[9600];			// Character buffer for bitstream of 8 bits each

int rows = 240;
int cols = 320;

/* This program demonstrates the use of the D5M camera with the DE1-SoC Board
 * It performs the following: 
 * 	1. Capture one frame of video when any key is pressed.
 * 	2. Display the captured frame when any key is pressed.		  
*/
/* Note: Set the switches SW1 and SW2 to high and rest of the switches to low for correct exposure timing while compiling and the loading the program in the Altera Monitor program.
*/

/*******************************
 	 Function calls
********************************/

// buffer - Waits for button press
void buffer(volatile int * Video_In_DMA_ptr, volatile int * KEY_ptr ){
	while (1)
	{
		if (*KEY_ptr != 0)				// check if any KEY was pressed
		{
			while (*KEY_ptr != 0);		// wait for pushbutton KEY release
			break;
		}
	}
}

// timestamp - Gets the current time in EST
void timestamp(){
	time_t t = time(NULL);				// sets up time for timestamp
	struct tm *tm = localtime(&t);
	strftime(timetext, sizeof(timetext), "%c", tm);	// string of timestamp
}

// printtext - Prints time/count to VGA
void printtext(char *time_ptr, char *count_ptr){
	count++;							// Keeps track of number of images captured
	timestamp();
	int offset = (55 << 7) + 5;			// Max: Y=60, X=80 | The Offset is (000000 0000000)base2
    while ( *(time_ptr)){
    	*((char*)0xC9000000+offset) = *(time_ptr);
    	++time_ptr;
    	++offset;
    }
	snprintf(counttext, sizeof(counttext), "Count: %d", count);
	offset = (53 << 7) + 5;				// Count is printed above time and date
	while ( *(count_ptr)){
		*((char*)0xC9000000+offset) = *(count_ptr);
		++count_ptr;
		++offset;
	}
}

// saveimage - Saves image from VGA to input (temp array)
void saveimage(volatile short * Video_Mem_ptr){
	int y,x;
	for (y = 0; y < 240; y++) {
		for (x = 0; x < 320; x++) {
			input[y][x] = *(Video_Mem_ptr + (y << 9) + x);
		}
	}
}

// writeimage - writes image to screen from input (temp array)
void writeimage(volatile short * Video_Mem_ptr){
	int y,x;
	for (y = 0; y < 240; y++) {
		for (x = 0; x < 320; x++) {
			*(Video_Mem_ptr + (y << 9) + x) = input[y][x];
		}
	}
}

// flip - flips the image along Y-axis
void flip(){
	int i, j;
	long rd2;
	rd2 = rows/2;
	for(i=0; i<rd2; i++){
		for(j=0; j<cols; j++){
			tempflip[rows-1-i][j] = input[i][j];
		}
	}
	for(i=rd2; i<rows; i++){
		for(j=0; j<cols; j++){
			tempflip[rows-1-i][j] = input[i][j];
		}
	}
	for(i=0; i<rows; i++){
		for(j=0; j<cols; j++){
			input[i][j] = tempflip[i][j];
		}
	} 
}

// mirror - mirrors the image along X-axis
void mirror(){
	int i, j;
	long cl2;
	cl2 = cols/2;
	for(i=0; i<rows; i++){
		for(j=0; j<cl2; j++){
			tempflip[i][cols-1-j] = input[i][j];
		} 
	}
	for(i=0; i<rows; i++){
		for(j=cl2; j<cols; j++){
			tempflip[i][cols-1-j] = input[i][j];
		}
	}
	for(i=0; i<rows; i++){
		for(j=0; j<cols; j++){
			input[i][j] = tempflip[i][j];
		}
	}
}

// black_white - Converts image to black/white and stores data in 8bit segments in prep for RLE compression. Image displayed is uncompressed.
void black_white(volatile short * Video_Mem_ptr){
	int y,x,R,G,B,sum;
	int threshold = 25;
	int track = 0;					// Tracks number of shifts so that 8 bits is entered into bitarr
	int index = 0;					// Stores a set of 8bits into arrays for (320x240)/8= 9600
	short temp;
	unsigned char pixelbuffer = 0;	// Stores 8bits segments of an image

	// Converts each color pixel into black or white and then stores 8bits into bitarray
	for (y = 0; y < 240; y++) {
		for (x = 0; x < 320; x++) {
			Video_Mem_ptr = FPGA_ONCHIP_BASE | (y <<10)| (x <<1);
			temp = *(short *) Video_Mem_ptr;
			R = (temp >> 11) & 0x1F;
			G = (temp >> 5) & 0x3F;
			B = temp & 0x1F;
			sum = R + G + B;
			if (sum >=  threshold){
			 *(short *) Video_Mem_ptr = 65535;
			 pixelbuffer = pixelbuffer >> 1 | 0x80;				
			}
			else{
			 *(short *) Video_Mem_ptr = 0;
			 pixelbuffer = pixelbuffer >> 1;
			}
			track++;
			if (track == 8){
				bitarr[index] = pixelbuffer;
				track = 0;
				//pixelbuffer = 0; // This isn't really needed since pixelbuffer shifts by 8 each time
				index++;
			}
		}
	}
	printf("============Uncompressed Black/White Image============\n");
	writeimage(Video_Mem_ptr);			// write image before compression 
}

// rle_compression takes 8bits at a time and outputs 32bits. Image is displayed is compressed data
void rle_compression(volatile short * Video_Mem_ptr){	
	short black_white[240][320];	// array for compressed black and white
	int x,y;
	int i = 0;
	int j = 0;
	int k = 0;
	int value = 0;					// This is the sign bit
	unsigned int number = 0;		// This is the number of 1's or 0's
	int times_compressed = 0;		// Keep track of times compressed
	int uncompressed = 0;
	float compressed_24 = 0;
	float compressed_32 = 0;
	float compression_ratio_24 = 0;
	float compression_ratio_32 = 0;
	unsigned int outpixelbuffer = 0;
	
	// Sends 8bit segments to RLE to be compressed. Output from RLE is stored in array
	for (y = 0; y < 9600;) {
		// Inputs to RLE
		if(!alt_read_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + FIFO_IN_FULL_PIO_BASE)){
			alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + ODATA_PIO_BASE, bitarr[y]);
			alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + FIFO_IN_WRITE_REQ_PIO_BASE, 1);
			alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + FIFO_IN_WRITE_REQ_PIO_BASE, 0);
			y++;
			if (y==9600){
				alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + RLE_FLUSH_PIO_BASE, 1);
				alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + RLE_FLUSH_PIO_BASE, 0);
			}
		}
		// Outputs from RLE
		if(!alt_read_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + RESULT_READY_PIO_BASE)){
			times_compressed++;
			alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + FIFO_OUT_READ_REQ_PIO_BASE, 1);
			outpixelbuffer = alt_read_word(ALT_FPGA_BRIDGE_LWH2F_OFST + IDATA_PIO_BASE);
			alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + FIFO_OUT_READ_REQ_PIO_BASE, 0);
			value = (outpixelbuffer & 0x00800000 ? 1 : 0);
			number = outpixelbuffer & 0x007FFFFF;
			//printf("Outpixelbuffer is: %X Y is:%d\nValue: %d Number: %d Index: %d\n",outpixelbuffer, y, value, number, index);
			//printf("Times Compressed: %d	 Y is:%d\n",times_compressed, y);
			for(i=0; i<number; i++){
				//printf("j: %d k: %d value: %d increment: %d\n", j, k, value,i);
				black_white[k][j] = value;
				j++;
				if(j == 320){
					k++;
					j=0;
				}
			}
		}		
	}
	
	// Takes the bits from the RLE and stores it to be displayed on VGA port
	for (y = 0; y < 240; y++) {
		for (x = 0; x < 320; x++) {
			Video_Mem_ptr = FPGA_ONCHIP_BASE | (y <<10)| (x <<1);
			if (black_white[y][x] == 1){
			 *(short *) Video_Mem_ptr = 65535;
			}
			else{
			 *(short *) Video_Mem_ptr = 0;
			}
		}
	}
	
	// Prints Compression Ratio on the Terminal interface
	uncompressed = 240*320;								// Total number of bits sent to RLE
	compressed_24 = times_compressed * 24;				// Total number of bits received from RLE (distorted to 24bits from 32bits)
	compression_ratio_24 = uncompressed/compressed_24;	// CR using distorted compression
	compressed_32 = times_compressed * 32;				// Total number of bits received from RLE (undistorted)
	compression_ratio_32 = uncompressed/compressed_32;	// CR using undistorted compression
	printf("Number of 32bits received: %d\n"
			"Uncompressed bits: %d\n"
			"=================32bits==================\n"
			"Compressed bits: %.0f\n"
			"Compression Ration: %.6f\n"
			"=================24bits==================\n"
			"Compressed bits: %.0f\n"
			"Compression Ration: %.6f\n"
			, times_compressed, uncompressed, compressed_32, compression_ratio_32, compressed_24, compression_ratio_24);
	printf("=============Compressed Black/White Image=============\n\n");
	writeimage(Video_Mem_ptr);			// write image after compression
}

// invert - Inverts image
void invert(volatile short * Video_Mem_ptr){
	int y,x,R,G,B,sum;
	int threshold = 30;
	short temp;
	for (y = 0; y < 240; y++) {
		for (x = 0; x < 320; x++) {
			Video_Mem_ptr = FPGA_ONCHIP_BASE | (y <<10)| (x <<1);
			temp = *(short *) Video_Mem_ptr;
			R = (temp >> 11) & 0x1F;
			G = (temp >> 5) & 0x3F;
			B = temp & 0x1F;
			sum = R + G + B;		
			if (sum < threshold){
			 *(short *) Video_Mem_ptr = 65535;
			}
			else{
			 *(short *) Video_Mem_ptr = 0;
			}
		}
	}
	writeimage(Video_Mem_ptr);			// write image
}

/* Main */
int main(void)
{
	volatile int * KEY_ptr			= (int *) KEY_BASE;
	volatile int * Video_In_DMA_ptr	= (int *) VIDEO_IN_BASE;
	volatile short * Video_Mem_ptr	= (short *) FPGA_ONCHIP_BASE;
	char *time_ptr = timetext;				// points to time character buffer
	char *count_ptr = counttext;			// points to count character buffer
	
    setenv("TZ", "EST5EDT", 1);				// Sets Timezone to EST
	*(Video_In_DMA_ptr + 3)	= 0x4;			// Enable the video
	
	while (1)
	{
//		/********** First Image FLIP **********/
//	//  Takes image and timestamps and displays on VGA
//		buffer(Video_In_DMA_ptr, KEY_ptr);	// Waits for button press
//		*(Video_In_DMA_ptr + 3) = 0x0;		// Disable the video to capture one frame
//		printtext(time_ptr, count_ptr);		// Prints timestamp to VGA
//	//  Flips image and displays on VGA
//		buffer(Video_In_DMA_ptr, KEY_ptr);	// Waits for button press
//		saveimage(Video_Mem_ptr);			// Save image from VGA
//		flip();								// Flip vertical image processing
//		writeimage(Video_Mem_ptr);			// Write image to VGA to show on screen
//	//  Restarts video for next image capture
//		buffer(Video_In_DMA_ptr, KEY_ptr);	// Waits for button press
//		*(Video_In_DMA_ptr + 3)	= 0x4;		// Enable the video	
//		
//		/********** Second Image MIRROR **********/
//	//  Takes image and timestamps and displays on VGA
//		buffer(Video_In_DMA_ptr, KEY_ptr);	// Waits for button press
//		*(Video_In_DMA_ptr + 3) = 0x0;		// Disable the video to capture one frame
//		printtext(time_ptr, count_ptr);		// Prints timestamp to VGA	
//	//  Mirrors image and displays on VGA
//		buffer(Video_In_DMA_ptr, KEY_ptr);	// Waits for button press
//		saveimage(Video_Mem_ptr);			// Save image from VGA
//		mirror();							// Mirror horizontal image processing
//		writeimage(Video_Mem_ptr);			// write image
//	//  Restarts video for next image capture
//		buffer(Video_In_DMA_ptr, KEY_ptr );	// waits for button press
//		*(Video_In_DMA_ptr + 3)	= 0x4;		// Enable the video in preparation for next image capture

	//  Initialize the RLE
		alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + RLE_FLUSH_PIO_BASE, 0);
		alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + RLE_RESET_BASE, 0);
		alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + RLE_RESET_BASE, 1);
		alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + RLE_RESET_BASE, 0);
		
		/**********  Compress Image BLACK/WHITE **********/
	//  Takes image and timestamps and displays on VGA
		buffer(Video_In_DMA_ptr, KEY_ptr);	// Waits for button press
		*(Video_In_DMA_ptr + 3) = 0x0;		// Disable the video to capture one frame
		printtext(time_ptr, count_ptr);		// Prints timestamp to VGA
	//  Converts image to black and white and displays on VGA
		buffer(Video_In_DMA_ptr, KEY_ptr);	// Waits for button press
		saveimage(Video_Mem_ptr);			// Save image from VGA
		black_white(Video_Mem_ptr);			// black/white image uncompressed
		buffer(Video_In_DMA_ptr, KEY_ptr );	// waits for button press
		rle_compression(Video_Mem_ptr);		// rle compression and display
	//  Restarts video for next image capture
		buffer(Video_In_DMA_ptr, KEY_ptr );	// waits for button press
		*(Video_In_DMA_ptr + 3)	= 0x4;		// Enable the video in preparation for next image capture
	}
}

