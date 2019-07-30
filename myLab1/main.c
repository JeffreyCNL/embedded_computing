/* Lab #1 - <Jeffrey Lee> */

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include <string.h>
#include <stdarg.h>
/* prototypes */
double double_in(char *prompt);
int printf_lcd(char *format,...);
char *fgets_keypad (char *buf, int buflen);

/* definitions */

double double_in(char *prompt){
	int err = 1; // declare an int called err as a threshold for while loop.
	char string[40]; // declare a string array with size of 40.
	double val; // declare a double variable name val will used as an address later.
	printf_lcd("\f"); // clear the LCD screen.
	while (err == 1){
		printf_lcd("\v%s",prompt); // print the prompt set when call this function
		if (fgets_keypad(string,40) == NULL){ // when the user input nothing.
			printf_lcd("\f \nShort.Try Again.");
		}else if  ((strpbrk(string,"[]") != NULL) || // when user input UP and DWN
				   (strpbrk(&string[1],"-") != NULL) || // when the minus sign is in index other than 0.
				   (strpbrk(string,"..") != NULL)){ // when user input more than one decimal point.
		printf_lcd("\f \nBad Key.Try Again."); // & return the actual address of a variable
		}else{
			err = 0; // set the threshold to zero so that we can jump out of the loop.
			sscanf(string,"%lf",&val); // compare character in string array with
			//and put in an address named val
			printf_lcd("\f The number is: \n");
		}
	}
	return val;
}
// Print string on LCD//
int printf_lcd(char *format,...){
		char string[40]; // declare string array as type char
		char *pointer; // declare char pointer
		int n; // declare variable n as type int
		va_list args; // set a va_list name args
		va_start(args,format);
		n = vsnprintf(string,40,format,args); // put input in string array and return the number of it.

		pointer = string; // put string as a pointer.
		while (*pointer != '\0'){ // if the pointer is not point to NULL.
			putchar_lcd(*pointer); // put character pointed to LCD screen.
			pointer++; // increment pointer.
		}
		va_end(args);
		return n;


	}
int main(int argc, char **argv) // argument count, argument value
{
	double vel;
	NiFpga_Status status;

    status = MyRio_Open();		    /*Open the myRIO NiFpga Session.*/
    if (MyRio_IsNotSuccess(status)) return status;

    vel = double_in("Enter Velocity: "); // call out function double_in
    printf("%lf",vel); // print in console.
    printf_lcd("%lf", vel); // print in LCD screen.


	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}
