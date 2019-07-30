/* Lab #<4> - <Jeffrey Lee> */

/* includes */
#include "stdio.h"
#include "Encoder.h"
#include "MyRio.h"
#include "DIO.h"
#include "me477.h"
#include <unistd.h>
#include "matlabfiles.h"
#include <string.h>
#include <math.h>
/* prototypes */

double double_in(char *prompt);
double vel(void);
void initializeSM(void);
void high(void);
void low(void);
void speed(void);
void stop(void);
NiFpga_Status EncoderC_initialize(NiFpga_Session myrio_session, MyRio_Encoder *channel);
uint32_t Encoder_Counter(MyRio_Encoder* channel);
//static void (*state_table[])(void) = {low,high,speed,stop};

/* global variables */
NiFpga_Session myrio_session;
MyRio_Encoder encC0;
#define IMAX 2400
static double buffer[IMAX];
static double *bp = buffer;


/* definitions */
/* Finite state machine */
typedef enum{LOW = 0,HIGH,SPEED,STOP,EXIT}State_Type; // note that the typedef has different
// variable name than the function name, or it will redeclare.
static void (*state_table[])(void) = {low,high,speed,stop}; // an array of pointers to named functions.
static State_Type curr_state;
static int clock, N, M;
MyRio_Dio Ch0,Ch6,Ch7;


double vel(void){
	static int cn, cn_1, counter = 1; // cn for the current counter, cn_1 for the previous one.
	// need to set the counter as static so that it won't keep changing when I called it.
	double speed;
	if (counter == 1){ // when encoder counter is first called, set the current one as previous one.
		cn = Encoder_Counter(&encC0);
		cn_1 = cn;
		counter++;
	}else{
		cn = Encoder_Counter(&encC0); // read the value again.
	}
	speed = cn - cn_1; //  calculate the speed of encoder by difference between current reading minus previous reading.
	cn_1 = cn;
	return (double) speed; // unit: BDI/BTI

}
void initializeSM(void){
		/* initialize the channel I am going to use. */
	Ch0.dir = DIOA_70DIR;
	Ch0.out = DIOA_70OUT;
	Ch0.in = DIOA_70IN;
	Ch0.bit = 0;

	Ch6.dir = DIOA_70DIR;
	Ch6.out = DIOA_70OUT;
	Ch6.in = DIOA_70IN;
	Ch6.bit = 6;

	Ch7.dir = DIOA_70DIR;
	Ch7.out = DIOA_70OUT;
	Ch7.in = DIOA_70IN;
	Ch7.bit = 7;

	EncoderC_initialize(myrio_session, &encC0); // initialize the encoder.
	Dio_WriteBit(&Ch0, NiFpga_True); // run=1, no current flow through the motor.
	curr_state = LOW; // set the initial state as low.
	clock = 0; // set the initial clock.
}
void low(void){
	if(clock == M){
		curr_state = HIGH;
		Dio_WriteBit(&Ch0,NiFpga_True); // set run to 1 for the high state.
	}
}
void speed(void){
	double speed,rpm;
	speed = vel(); //  get the count from encoder (BDI/BTI).
	rpm = (speed*60)/(2000*N*0.005); // each delay has 5ms delay, we have N of it.
	// translate the counter from encoder to rpm of the motor.
	curr_state = LOW;

//	printf_lcd("\fSpeed %g rpm",rpm);
	if(bp < buffer + IMAX){
		*bp++ = rpm; // store the rpm into buffer by buffer pointer
		// so that we can print out the step response from Matlab.
	}
}
void high(void){
	if(clock == N){
		clock = 0; // set the clock to zero.
		Dio_WriteBit(&Ch0,NiFpga_False); // set run=0.
		if (Dio_ReadBit(&Ch7) == NiFpga_False){
			curr_state = SPEED; // when Ch7 is 0, we should print out the speed on LCD.
		}
		else if(Dio_ReadBit(&Ch6) == NiFpga_False){
			curr_state = STOP; // if Ch6 is pressed, the motor should stop.
		}
		else{
			curr_state = LOW;
		}
	}
}
void stop(void){
	Dio_WriteBit(&Ch0,NiFpga_True); // set run to 1.
	printf_lcd("\fStopping");
	curr_state = EXIT;

	double Npar,Mpar;
	Npar = (double)N;
	Mpar = (double)M;

	MATFILE *mf;
	int err;
	mf = openmatfile("Lab4.mat",&err);
	if (!mf){
		printf("Can't open mat file %d\n",err);
	}
	matfile_addmatrix(mf,"speed",buffer,IMAX,1,0);
	matfile_addmatrix(mf,"N",&Npar,1,1,0);
	matfile_addmatrix(mf,"M",&Mpar,1,1,0);
	matfile_addstring(mf,"myName","Jeffrey Lee");
	matfile_close(mf);


}
void wait(void){
	uint32_t i;
	i = 417000;
	while(i>0){
		i--;
	}
	return;
}


int main(int argc, char **argv)
{
	NiFpga_Status status;

    status = MyRio_Open();		    /*Open the myRIO NiFpga Session.*/
    if (MyRio_IsNotSuccess(status)) return status;

    //my code here
    initializeSM(); // initialize finite state machine.
    N = double_in("Enter N(for BTI): "); //  determine the BTI by the user.
    M = double_in("Enter M(for how long the current pass through motor): "); // decide when the signal is on.
    while(1){ // start the state transition loop.
    	state_table[curr_state](); // call the current state.
    	wait(); // wait a fixed time interval.
    	clock++; // keep track of clock count.
    	if (curr_state == EXIT){
    		break;
    	}

    }

	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;

}
