/* Lab <3> - <Jeffrey Lee> */

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include "UART.h"
#include "DIO.h"
#include <time.h>
#include <stdarg.h>
#include <string.h>

/* prototypes */
int putchar_lcd(int value);
char getkey(void);
void set_col_high_z(void);
void wait(void);

/* definitions */
#define ROW_LEN 4
#define COL_LEN 4
#define BUF_LEN 80
static MyRio_Uart uart;
MyRio_Dio Ch[8];
int putchar_lcd(int value){
	NiFpga_Status status;
	const int buf_len = 80;
	uint8_t writeS[buf_len];
	size_t nData = 0;
	uint8_t *pointer;

	uart.name = "ASRL2::INSTR";            // UART on Connector B
	uart.defaultRM = 0;                    // def. resource manager
	uart.session = 0;                      // session referencet
	status = Uart_Open(&uart,              // port information
			           19200,              // baud rate
			           8,                  // no. of data bits
			           Uart_StopBits1_0,   // 1 stop bit
			           Uart_ParityNone);   // No parity
	if (status < (VI_SUCCESS)){            // check the initialized status
		return EOF;
	}

	if (value < 0 || value > 255){         // check if the value are within the table
		return EOF;
	}
	if (value== 128){					   // If the value entered is '\v'
		nData++;					           // Value size is 1
		*writeS = 128; 				           // Store number into array
		status = Uart_Write(&uart,writeS,nData);
	}
	pointer = writeS;                      // put pointer in the beginning of array.
	*pointer = value;                      // save value to where pointer at.
	pointer++;                             // increment the pointer.
	nData++;                               // increment the no. of data
	status = Uart_Write(&uart,             // write the value on lcd.
						writeS,
						nData);
	if (status < VI_SUCCESS){              // check the write status.
		return EOF;
	}

	return value;
}

void set_col_high_z(void){
	int i;
	for (i = 0; i < COL_LEN; i++){
		Dio_WriteBit(&Ch[i+4],NiFpga_True);// set every column bit to high.
	}
}

void wait(void){
	uint32_t i;
	i = 11700000;
	while(i > 0){
		i--;
	}
	return;
}


char getkey(void){
	char table [4][4] = {{'1','2','3', UP},
						 {'4','5','6', DN},
						 {'7','8','9',ENT},
						 {'0','.','-',DEL}};

	NiFpga_Bool row_bit = NiFpga_True;      // initial row is high
	NiFpga_Bool col_bit = NiFpga_True;      // initial column is high
	int i,j,k;

	for(k = 0;k < 8;k++){                   // channel initialization.
		Ch[k].dir = DIOB_70DIR;
		Ch[k].out = DIOB_70OUT;
		Ch[k].in = DIOB_70IN;
		Ch[k].bit = k;
	}                                          // initialize the 8 digital channels.

	while ((row_bit != NiFpga_False) || (col_bit != NiFpga_False)){
		for(j = 0; j < COL_LEN; j++){
			set_col_high_z();                   // set all columns to Hi-Z
			Dio_WriteBit(&Ch[j],NiFpga_False);  // write jth column low
			for (i = 0;i < ROW_LEN;i++){
				row_bit = Dio_ReadBit(&Ch[i+4]);// read the ith row
				if (row_bit == NiFpga_False){
//					return i;
					break;
				}
			}
			col_bit = Dio_ReadBit(&Ch[j]);
			if (col_bit == NiFpga_False && row_bit == NiFpga_False){
//				return j;
				break;
			}
		}
		wait();
	}
	while (Dio_ReadBit(&Ch[j+4]) == NiFpga_False){
		wait();
	}
	return table[i][j];



}


int main(int argc, char **argv)
{
	NiFpga_Status status;

    status = MyRio_Open();		    /*Open the myRIO NiFpga Session.*/
    if (MyRio_IsNotSuccess(status)) return status;

//   /* Test 1: test two functions call*/
//    char key;
//    printf_lcd("\f");
//    putchar_lcd('k'); // use '' instead of "" since I take (int value) not (char value)
//    key = getkey();
//    putchar_lcd(key);

    /* Test 2: collect entire string using fgets_keypad() */
//    char input[BUF_LEN];
//    char *data;
//    data = fgets_keypad(input,BUF_LEN);
//    printf(data);
//    printf_lcd(data);

    /* Test 3: Write entire string and test four escape sequences */
    printf_lcd("\f");
    printf_lcd("Hello World");
//    printf_lcd("\b");
    printf_lcd("\v");
    printf_lcd("\n");





	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}
