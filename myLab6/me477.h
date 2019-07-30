/*
 * me477.h
 *
 *  Created on: Sep 18, 2015
 *      Author: garbini
 */

#include "AIO.h"
#include "MyRio.h"
#include "Encoder.h"

#ifndef ME477_H_
#define ME477_H_
#endif /* ME477_H_ */

#define ENT		10
#define	DEL		8
#define UP		91
#define DN		93

int	putchar_lcd(int c);
int printf_lcd(char *format, ...);
int scanf_keypad(char *format, ...);
char getkey(void);
int	msDelay(int tms);
int getchar_keypad(void);
float	float_in(char *prompt);
double	double_in(char *prompt);
char * fgets_keypad(char *buf, int buflen);
void AIO_initialize(MyRio_Aio *CI0p, MyRio_Aio *CO0p);
NiFpga_Status	EncoderC_initialize(NiFpga_Session myrio_session, MyRio_Encoder *encC0);
