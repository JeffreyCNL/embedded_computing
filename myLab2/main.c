/* Lab #<2> - <Jeffrey Lee> */

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include <string.h>
#include <stdbool.h>


/* prototypes */
int getchar_keypad(void);
/* definitions */
int n = 0;
#define buf_len 80 // use define so that I can just change the size once.
//int buf_len = 80;
char buffer[buf_len]; // character array.
char *pointer;
char key;
char input[buf_len];
char *data;
int threshold = 1;
int i = 1;
int getchar_keypad(void){

	if (n == 0){
		pointer = buffer; // point to the start of buffer array.
		//key = getkey();
		while ((key = getkey()) != ENT && n < buf_len){ // before I hit enter or it's over the buffer limit.
			// it will always stay in this loop.
			if (key == DEL && n == 0){ // if there's nothing inside. Delete shouldn't work.
				continue;
			}
			else if(key == DEL){
				putchar_lcd('\b'); // move cursor left.
				putchar_lcd(' '); // insert it as a space.
				putchar_lcd('\b'); // since the cursor move right again. We need to put it left again.
				pointer--; // this is important since we want to clear the delete part
				n--;       // so it won't print out on the screen.
			}
			else{
				*pointer = key; // set key to the buffer. Put it to where pointer at.
				pointer++;
				n++;
				putchar_lcd(key);
			}
		}
		n++; // should include enter.
		pointer = buffer; // set pointer to the start of the buffer again.
	}
	if (n > 1){
		n--; // fgets_keypad will save the key into input array.
		return *pointer++; // move pointer right to keep reading the data.
	}else { // once n = 1, which means only ENTR inside.
		n--; // set the character number to zero.
		return EOF; //End OF File.
	}
}
int main(int argc, char **argv)
{
	NiFpga_Status status;

    status = MyRio_Open();		    /*Open the myRIO NiFpga Session.*/
    if (MyRio_IsNotSuccess(status)) return status;

    //my code here

    while (threshold == 1){
    	printf_lcd("\fEnter a number:"); // prompt
    	data = fgets_keypad(input,buf_len); // safe the data into input.
    	printf_lcd ("\f%d input is:%s",i,input);
    	printf_lcd("\nHit ""ENTR"" to leave, or hit ""-"" to keep inputting data");
    	i++;
    	//if (strpbrk(input,"ENT") != NULL)
    	if (getkey() == ENT){
    		printf_lcd("\f");
    		threshold = 0;
    	}
    }

	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}
