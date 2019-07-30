/* Lab #<6> - <Jeffrey Lee> */

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include <string.h>
#include <pthread.h>
#include "IRQConfigure.h"
#include "TimerIRQ.h"
#include "matlabfiles.h"

/* prototypes */
double Aio_Read(MyRio_Aio* channel);             // read the analog input.

void Aio_Write(MyRio_Aio* channel, double value);// send output value to analog output.

void *Timer_Irq_Thread(void* resource);          // ISR

struct biquad{                                      // this should be defined before using.
	double b0; double b1; double b2; // numerator
	double a0; double a1; double a2; // denominator
	double x0; double x1; double x2; // input
	double y1; double y2;            // output
};


double cascade(double xin,        // input
			   struct biquad *fa, // biquad array
			   int ns,            // no. segments
			   double ymin,       // min output
			   double ymax);      // max output


/* definitions */
int myFilter_ns = 2; // No. of sections
uint32_t timeoutValue = 500; // 5ms
#define IMAX 500             // save 500 data points.
static double inBuffer[IMAX], outBuffer[IMAX];
static double *iBp = inBuffer, *oBp = outBuffer;
// saturate the output into acceptable range. This is a macro.
#define SATURATE(x,lo,hi)((x) < (lo)?(lo):(x)>(hi)?(hi):(x))

// globally defined to let thread to communitcate.
typedef struct{
	NiFpga_IrqContext irqContext; // IRQ context reserved.
	NiFpga_Bool irqThreadRdy;     // IRQ thread ready flag.
}ThreadResource;

double cascade(double xin, struct biquad *fa, int ns, double ymin, double ymax ){
	struct biquad* f;
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

	static struct biquad myFilter[] = {
			{1.0000e+00, 9.9999e-01, 0.0000e+00,
			 1.0000e+00, -8.8177e-01, 0.0000e+00, 0, 0, 0, 0, 0},
			{2.1878e-04, 4.3755e-04, 2.1878e-04,
			 1.0000e+00, -1.8674e+00, 8.8220e-01, 0, 0, 0, 0, 0}
	};


	MyRio_Aio CI0,CO0;
	double  curr_out, curr_in;
	int ns = 2;
	double ymin = -10, ymax = 9.995; // voltage range.

 	AIO_initialize(&CI0, &CO0); // initialize input and output interface.
    Aio_Write(&CO0, 0);         // set the first output zero.

    //
	while (threadResource->irqThreadRdy == NiFpga_True){
		uint32_t irqAssert = 0;
		extern NiFpga_Session myrio_session;

		// wait IRQ to assert.
		Irq_Wait(threadResource->irqContext,
				 TIMERIRQNO,
				 &irqAssert,
				 (NiFpga_Bool*)&(threadResource->irqThreadRdy));

		// write timer write register.
		// write TRUE to the timer set time register.
		NiFpga_WriteU32(myrio_session,IRQTIMERWRITE,timeoutValue);
		NiFpga_WriteBool(myrio_session,IRQTIMERSETTIME, NiFpga_True);

		//
		if (irqAssert){
			curr_in = Aio_Read(&CI0); // read the input and save in current input.
			if(iBp < inBuffer + IMAX){
				*iBp++ = curr_in; // store the curr_in into buffer by buffer pointer
			}
			// calculate the current value of the output by biquad cascade.
			// use the filter, low-pass. send the value to current output.
			curr_out = cascade(curr_in,myFilter,ns,ymin,ymax);
			if(oBp < outBuffer + IMAX){
				*oBp++ = curr_out; // store the curr_out into buffer by buffer pointer
			}
			// send the output to analog output.
			Aio_Write(&CO0, curr_out);
			Irq_Acknowledge(irqAssert);
		}

	}
	pthread_exit(NULL);
	return NULL;
};

int main(int argc, char **argv)
{
	NiFpga_Status status;

    status = MyRio_Open();		    /*Open the myRIO NiFpga Session.*/
    if (MyRio_IsNotSuccess(status)) return status;

    //my code here
    MyRio_IrqTimer irqTimer0;
    ThreadResource irqThread0;
    pthread_t thread;
    char key;

    // set up and enable the timer IRQ interrupt.
   	// Registers corresponding to the IRQ channel
   	irqTimer0.timerWrite = IRQTIMERWRITE;
   	irqTimer0.timerSet = IRQTIMERSETTIME;
   	timeoutValue = 500; //  0.5 ms

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


    // wait for the DEL key to terminate the program.
    while (key != DEL){
    	key = getkey();
    }

	// set the flag to ThreadResource to terminate the new thread.
   	irqThread0.irqThreadRdy = NiFpga_False;
   	pthread_join(thread,NULL);

	MATFILE *mf;
	int err;
	mf = openmatfile("Lab6_200Hz.mat",&err);
	if (!mf){
		printf("Can't open mat file %d\n",err);
	}
		matfile_addmatrix(mf,"input",inBuffer,IMAX,1,0);
		matfile_addmatrix(mf,"output",outBuffer,IMAX,1,0);
		matfile_addstring(mf,"myName","Jeffrey Lee");
		matfile_close(mf);

	// Unregister Irq
	Irq_UnregisterTimerIrq(&irqTimer0,
						    irqThread0.irqContext);



	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}
