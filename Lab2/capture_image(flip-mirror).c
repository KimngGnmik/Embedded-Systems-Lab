#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define KEY_BASE              0xFF200050
#define VIDEO_IN_BASE         0xFF203060
#define FPGA_ONCHIP_BASE      0xC8000000

short input[240][320];		// [Rows][Cols]
int count = 0;				// Keeps track of image #
char s[64];


/* This program demonstrates the use of the D5M camera with the DE1-SoC Board
 * It performs the following: 
 * 	1. Capture one frame of video when any key is pressed.
 * 	2. Display the captured frame when any key is pressed.		  
*/
/* Note: Set the switches SW1 and SW2 to high and rest of the switches to low for correct exposure timing while compiling and the loading the program in the Altera Monitor program.
*/


/* Function calls */

// Waits for button press
void buffer(volatile int * Video_In_DMA_ptr, volatile int * KEY_ptr ){
	while (1)
	{
		if (*KEY_ptr != 0)					// check if any KEY was pressed
		{
			*(Video_In_DMA_ptr + 3) = 0x0;	// Disable the video to capture one frame
			while (*KEY_ptr != 0);			// wait for pushbutton KEY release
			break;
		}
	}
}

// Function prints text to VGA
void printtext(char *text_ptr, int offset){
    while ( *(text_ptr)){
    	*((char*)0xC9000000+offset) = *(text_ptr);
    	++text_ptr;
    	++offset;
    }
}

// This saves image from VGA to temp location
void saveimage(volatile short * Video_Mem_ptr){
	int y,x;
	for (y = 0; y < 240; y++) {
		for (x = 0; x < 320; x++) {
			input[y][x] = *(Video_Mem_ptr + (y << 9) + x);
		}
	}
}

//This writes image to screen
void writeimage(volatile short * Video_Mem_ptr){
	int y,x;
	for (y = 0; y < 240; y++) {
		for (x = 0; x < 320; x++) {
			*(Video_Mem_ptr + (y << 9) + x) = input[y][x];
		}
	}
}

int main(void)
{
	volatile int * KEY_ptr			= (int *) KEY_BASE;
	volatile int * Video_In_DMA_ptr	= (int *) VIDEO_IN_BASE;
	volatile short * Video_Mem_ptr	= (short *) FPGA_ONCHIP_BASE;
	
	//Temp variables for flip/mirror
	int i, j;
	int rows = 240;
	int cols = 320;
    long rd2;
	long cl2;
    short tempflip[240][320];
	
    setenv("TZ", "EST5EDT", 1);					// Sets Timezone to EST
	*(Video_In_DMA_ptr + 3)	= 0x4;				// Enable the video
	
	while (1)
	{
		buffer(Video_In_DMA_ptr, KEY_ptr );	// Stops video
		
		count++;						// Keeps track of number of images captured
		/* Display the time in EST and UTC */
		time_t t = time(NULL);
		struct tm *tm = localtime(&t);
		strftime(s, sizeof(s), "%c", tm);

		int offset = (55 << 7) + 5;		// Max: Y=60, X=80 | The Offset is (000000 0000000)base2
		char *text_ptr = s;
		
		printtext(text_ptr, offset);	// Prints Time Date to VGA
		
		char *cha = s;
		snprintf(cha, 12, "Count: %d", count);
		text_ptr = s;
		offset = (53 << 7) + 5;			//offset = (55 << 7) + 35;   count will be next to time/date

		printtext(text_ptr, offset);	// Prints Time Date to VGA
		
		buffer(Video_In_DMA_ptr, KEY_ptr );	// Stops video for buffer
		saveimage(Video_Mem_ptr);		// Save image
		
//This is FLIP
	    rd2 = rows/2;
	    for(i=0; i<rd2; i++){
	    	for(j=0; j<cols; j++){
	    		tempflip[rows-1-i][j] = input[i][j];
	    	} /* ends loop over j */
	    } /* ends loop over i */
	    
	    for(i=rd2; i<rows; i++){
	    	for(j=0; j<cols; j++){
	    		tempflip[rows-1-i][j] = input[i][j];
	    	} /* ends loop over j */
	    } /* ends loop over i */
	    
	    for(i=0; i<rows; i++){
	    	for(j=0; j<cols; j++){
	    		input[i][j] = tempflip[i][j];
	    	} /* ends loop over j */
	    } /* ends loop over i */
//This ends flip
		writeimage(Video_Mem_ptr);			// write image
		buffer(Video_In_DMA_ptr, KEY_ptr );	// waits for button press
		
//This is FLIP back
		rd2 = rows/2;
		for(i=0; i<rd2; i++){
			for(j=0; j<cols; j++){
				tempflip[rows-1-i][j] = input[i][j];
			} /* ends loop over j */
		} /* ends loop over i */
		
		for(i=rd2; i<rows; i++){
			for(j=0; j<cols; j++){
				tempflip[rows-1-i][j] = input[i][j];
			} /* ends loop over j */
		} /* ends loop over i */
		
		for(i=0; i<rows; i++){
			for(j=0; j<cols; j++){
				input[i][j] = tempflip[i][j];
			} /* ends loop over j */
		} /* ends loop over i */
//This ends flip back
		
//This is Mirror
		cl2 = cols/2;
		for(i=0; i<rows; i++){
			for(j=0; j<cl2; j++){
				tempflip[i][cols-1-j] = input[i][j];
			} /* ends loop over j */
		} /* ends loop over i */
		
		for(i=0; i<rows; i++){
			for(j=cl2; j<cols; j++){
				tempflip[i][cols-1-j] = input[i][j];
			} /* ends loop over j */
		} /* ends loop over i */
		
		for(i=0; i<rows; i++){
			for(j=0; j<cols; j++){
				input[i][j] = tempflip[i][j];
			} /* ends loop over j */
		} /* ends loop over i */
//This ends mirror
		
		writeimage(Video_Mem_ptr);		// write image
		buffer(Video_In_DMA_ptr, KEY_ptr );	// waits for button press
		*(Video_In_DMA_ptr + 3)	= 0x4;	// Enable the video in preparation for next image capture
	}
}

