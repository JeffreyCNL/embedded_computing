/* Lab #<7> - <Jeffrey Lee> */

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include <string.h>
#include "ctable2.h"
#include "matlabfiles.h"
#include "Encoder.h"
#include "TimerIRQ.h"
#include "IRQConfigure.h"
#include "AIO.h"
#include <pthread.h>

/*Global variable*/
#define PI 3.14159265358979323846
#define SATURATE(x,lo,hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))
#define IMAX 250
#define Kvi 0.41
#define Kt 0.11
int ns = 1; // T - us; f_s = 2000 Hz

MyRio_Aio CI0;
MyRio_Aio CO0;
double timeoutValue = 5000; // ms
MyRio_Encoder encC0;
NiFpga_Session myrio_session;
static double vel_buffer[IMAX], torq_buffer[IMAX];
static double *vel_pt = vel_buffer, *torq_pt = torq_buffer;


/* prototypes */

struct biquad{                                      // this should be defined before using.
	double b0; double b1; double b2; // numerator
	double a0; double a1; double a2; // denominator
	double x0; double x1; double x2; // input
	double y1; double y2;            // output
};

void *Timer_Irq_Thread(void* resource);

double vel(void);

double cascade(double xin,        // input
			   struct biquad *fa, // biquad array
			   int ns,            // no. segments
			   double ymin,       // min output
			   double ymax);      // max output

/* definitions */
typedef struct {
	NiFpga_IrqContext irqContext; //context
	table *a_table;      		  // table
	NiFpga_Bool irqThreadRdy;	  // ready flag
}ThreadResource;

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
	return (double) speed;
}

double cascade(double xin, struct biquad *fa, int ns, double ymin, double ymax){
	struct biquad *f;
	int i;
	f = fa;
	double y0;
	y0 = xin;
	for (i = 1; i <= ns; i++){
		f->x0 = y0;
		y0 = (f->b0*f->x0 + f->b1*f->x1 + f->b2*f->x2 - f->a1*f->y1 - f->a2*f->y2)/f->a0;
		f->x2 = f->x1;f->x1 = f->x0;f->y2 = f->y1; f->y1 = y0;
		f++;
	}
	y0 = SATURATE(y0,ymin,ymax);
	return y0;
}

void *Timer_Irq_Thread(void* resource){
	// cast its input argument into appropriate form.
	ThreadResource* threadResource = (ThreadResource*) resource;
	double *vref = &((threadResource->a_table+0)->value);
	double *vact = &((threadResource->a_table+1)->value);
	double *vdaout = &((threadResource->a_table+2)->value);
	double *kp = &((threadResource->a_table+3)->value);
	double *ki = &((threadResource->a_table+4)->value);
	double *bti = &((threadResource->a_table+5)->value);
	double vref_PREsave, vref_current;


	MyRio_Aio CI0,CO0;
	static struct biquad myFilter[] = {
			{0.0, 0.0, 0.0, 1.0, -1.0, 0,0,0,0,0}
	};
	struct biquad *filter = myFilter;

	extern NiFpga_Session myrio_session; // get the myrio_session outside.
 	AIO_initialize(&CI0, &CO0); // initialize input and output interface.
    Aio_Write(&CO0, 0.0);         // set the motor voltage zero.


	while (threadResource->irqThreadRdy == NiFpga_True){
		uint32_t irqAssert = 0;
		double ymin = -7.5, ymax = 7.5; // voltage range.
		double error, torq, T;
		double vref_RPS, vact_RPS, vout;
		double vref_PRE = *vref*(PI/30);


		// wait IRQ to assert.
		Irq_Wait(threadResource->irqContext,
				 TIMERIRQNO,
				 &irqAssert,
				 (NiFpga_Bool*)&(threadResource->irqThreadRdy));
		// write timer write register.
		// write TRUE to the timer set time register.
		NiFpga_WriteU32(myrio_session,IRQTIMERWRITE,timeoutValue);
		NiFpga_WriteBool(myrio_session,IRQTIMERSETTIME, NiFpga_True);

		if (irqAssert){
			// measure the velocity of the motor
			*vact = vel()*30/(*bti); // convert to r/s
			vact_RPS = *vact*(PI/30);
			vref_RPS = *vref*(PI/30);

			// compute current coefficient
			T = *bti/1000;
			filter -> b0 = *kp + 0.5*(*ki)*T;
			filter -> b1 = -(*kp) + 0.5*(*ki)*T;
			filter -> a0 = 1.0;
			filter -> a1 = -1.0;

			// compute the current error
			error = vref_RPS - vact_RPS;

			// using cascade to compute the control value.
			vout = cascade(error, myFilter, ns, ymin, ymax);
			torq = Kvi*Kt*vout;

			// send the output to analog output.
			Aio_Write(&CO0, vout);

			*vdaout = vout*1000.0; // vout(mV)

			// Store the output data in the buffer
			if(vel_pt < (vel_buffer+IMAX))
			{
				*vel_pt++ = vact_RPS;
			}
			if (torq_pt < (torq_buffer+IMAX)){
				*torq_pt++ = torq;
			}
			if( vref_RPS != vref_PRE)
			{
				vel_pt = vel_buffer; 		// Restart the buffer
				torq_pt = torq_buffer; 		// Restart the buffer
				vref_PREsave = vref_PRE;
				vref_PRE = vref_RPS;
				vref_current = vref_RPS;
			}

			Irq_Acknowledge(irqAssert);

			}
		//create Matlab file
		MATFILE *mf;
		int err;
		mf = openmatfile("Lab7_Ki_10.mat",&err);
		if (!mf){
			printf("Can't open mat file %d\n",err);
		}

		matfile_addstring(mf,"myName","Jeffrey Lee");
		matfile_addmatrix(mf,"actual_vel",vel_buffer,IMAX,1,0);
		matfile_addmatrix(mf,"torque",torq_buffer,IMAX,1,0);
		matfile_addmatrix(mf,"V_Pre",&vref_PREsave, 1,1,0);
		matfile_addmatrix(mf,"V_Curr",&vref_current, 1,1,0);
		matfile_addmatrix(mf,"Kp",kp,1,1,0);
		matfile_addmatrix(mf,"Ki",ki,1,1,0);
		matfile_addmatrix(mf,"BTI",bti,1,1,0);
		matfile_close(mf);

	}
	pthread_exit(NULL);
	return NULL;
};

int main(int argc, char **argv){
	NiFpga_Status status;

    status = MyRio_Open();		    /*Open the myRIO NiFpga Session.*/
    if (MyRio_IsNotSuccess(status)) return status;

    //my code here
    char *TableTitle = "Vel Control Table";
    int nval = 6;
    MyRio_IrqTimer irqTimer0;
    ThreadResource irqThread0;
    pthread_t thread;
    char key;



    // initialize the table editor variables.
    table my_table[] = {
    		{"V_ref: (rpm)", 1, 0.0},
    		{"V_act: (rpm)", 0, 0.0},
    		{"VDAout: (mV)", 0, 0.0},
    		{"Kp: (V-s/r)", 1, 0.0},
    		{"Ki: (V/r)", 1, 0.0},
    		{"BTI: ms", 1, 5.0}
    };
    irqThread0.a_table = my_table;

     // set up and enable the timer IRQ interrupt.
	// Registers corresponding to the IRQ channel
	irqTimer0.timerWrite = IRQTIMERWRITE;
	irqTimer0.timerSet = IRQTIMERSETTIME;
	EncoderC_initialize(myrio_session, &encC0); // initialize the encoder.


	Irq_RegisterTimerIrq(&irqTimer0,
						 &irqThread0.irqContext,
						 timeoutValue);

	// Set the indicator to allow the new thread.
	irqThread0.irqThreadRdy = NiFpga_True;

	// Create new thread to catch the IRQ.
	pthread_create(&thread,
			   	   NULL,
       			   Timer_Irq_Thread,
       			   &irqThread0);

	// call the table editor.
	ctable2(TableTitle, my_table, nval);

	// wait for the DEL key to terminate the program.
	while (key != DEL){
		key = getkey();
	}

	// set the flag to ThreadResource to terminate the new thread.
	irqThread0.irqThreadRdy = NiFpga_False;
	pthread_join(thread,NULL);

	// Unregister Irq
	Irq_UnregisterTimerIrq(&irqTimer0,
						    irqThread0.irqContext);
	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}

