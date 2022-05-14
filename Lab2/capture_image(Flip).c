#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define KEY_BASE              0xFF200050
#define VIDEO_IN_BASE         0xFF203060
#define FPGA_ONCHIP_BASE      0xC8000000


/* This program demonstrates the use of the D5M camera with the DE1-SoC Board
 * It performs the following: 
 * 	1. Capture one frame of video when any key is pressed.
 * 	2. Display the captured frame when any key is pressed.		  
*/
/* Note: Set the switches SW1 and SW2 to high and rest of the switches to low for correct exposure timing while compiling and the loading the program in the Altera Monitor program.
*/


int main(void)
{
	volatile int * KEY_ptr			= (int *) KEY_BASE;
	volatile int * Video_In_DMA_ptr	= (int *) VIDEO_IN_BASE;
	volatile short * Video_Mem_ptr	= (short *) FPGA_ONCHIP_BASE;

	/* Define temporary variables */
	int x, y;
	int count = 0;								// Keeps track of image #
    char s[64];
	short input[240][320];						// [Rows][Cols]
	
    setenv("TZ", "EST5EDT", 1);					// Sets Timezone to EST
	*(Video_In_DMA_ptr + 3)	= 0x4;				// Enable the video
	
	
	/* Functions */
	
	//Flips Vertical
//	void flip_Vertical(image, rows, cols ) {
//		int i, j;
//	    long rd2;
//	    short tempflip[240][320];
//	    
//	    rd2 = rows/2;
//	    for(i=0; i<rd2; i++){
//	    	for(j=0; j<cols; j++){
//	    		tempflip[rows-1-i][j] = image[i][j];
//	    	} /* ends loop over j */
//	    } /* ends loop over i */
//	    
//	    for(i=rd2; i<rows; i++){
//	    	for(j=0; j<cols; j++){
//	    		tempflip[rows-1-i][j] = image[i][j];
//	    	} /* ends loop over j */
//	    } /* ends loop over i */
//	    
//	    for(i=0; i<rows; i++){
//	    	for(j=0; j<cols; j++){
//	    		image[i][j] = tempflip[i][j];
//	    	} /* ends loop over j */
//	    } /* ends loop over i */
//	} /* ends flip_image_array */

	while (1)
	{
		while (1)
		{
			if (*KEY_ptr != 0)					// check if any KEY was pressed
			{
				*(Video_In_DMA_ptr + 3) = 0x0;	// Disable the video to capture one frame
				while (*KEY_ptr != 0);			// wait for pushbutton KEY release
				count++;						// Keeps track of number of images captured
				time_t t = time(NULL);
				struct tm *tm = localtime(&t);
				strftime(s, sizeof(s), "%c", tm);
				break;
			}
		}
		
		/* Display the time in EST and UTC */
		//printf ("Count: %i, Time: %s", count, ctime(&rawtime));

		int offset = (55 << 7) + 5;		// Max: Y=60, X=80 | The Offset is (000000 0000000)base2
		int location;
		char *text_ptr = s;
		while ( *(text_ptr)){
		   location = 0xC9000000;
		   volatile char * address = (char*) location;
		   *(address+offset) = *(text_ptr);
		   ++text_ptr;
		   ++offset;
		}
		
		char *cha = s;
		snprintf(cha, 12, "Count: %d", count);
		text_ptr = s;
		offset = (53 << 7) + 5;			//offset = (55 << 7) + 35;   count will be next to time/date


		while ( *(text_ptr)){
		   location = 0xC9000000;
		   volatile char * address = (char*) location;
		   *(address+offset) = *(text_ptr);
		   ++text_ptr;
		   ++offset;
		}

		while (1)
		{
			if (*KEY_ptr != 0)				// check if any KEY was pressed
			{
				while (*KEY_ptr != 0);			// wait for pushbutton KEY release
				break;
			}
		}


		// This saves
		for (y = 0; y < 240; y++) {
			for (x = 0; x < 320; x++) {
				input[y][x] = *(Video_Mem_ptr + (y << 9) + x);
				//*(Video_Mem_ptr + (y << 9) + x) = temp2;
			}
		}

//TESTING CODE HERE
		
		int i, j;
		int rows = 240;
		int cols = 320;
	    long rd2;
	    short tempflip[240][320];
	    
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

		
		
		
		
//END TESTING CODE HERE
		
		// This writes
		for (y = 0; y < 240; y++) {
			for (x = 0; x < 320; x++) {
				//input[x][y] = *(Video_Mem_ptr + (y << 9) + x);
				*(Video_Mem_ptr + (y << 9) + x) = input[y][x];
			}
		}
		
		while (1)
		{
			if (*KEY_ptr != 0)				// check if any KEY was pressed
			{
				*(Video_In_DMA_ptr + 3)	= 0x4;	// Enable the video in preparation for next image capture
				while (*KEY_ptr != 0);			// wait for pushbutton KEY release
				break;
			}
		}
		
	}
}


