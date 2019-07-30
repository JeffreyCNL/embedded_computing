/* Lab #<8> - <Jeffrey Lee> */

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include <string.h>
#include "ctable2.h"
#include "TimerIRQ.h"
#include "matlabfiles.h"
#include "Encoder.h"
#include "math.h"
#include "AIO.h"
#include <pthread.h>
#include "IRQConfigure.h"

/* global variables*/
MyRio_Aio CI0;
MyRio_Aio CO0;
MyRio_Encoder encC0;
NiFpga_Session myrio_session;
#define SATURATE(x,lo,hi)((x) < (lo)?(lo):(x)>(hi)?(hi):(x))
#define IMAX 4000
#define Kvi 0.41
#define Kt 0.11
#define PI 3.14159265358979
static double pref_buffer[IMAX], pact_buffer[IMAX], torq_buffer[IMAX];
static double *pref_pt = pref_buffer, *pact_pt = pact_buffer;
static double *torq_pt = torq_buffer;

//seg
typedef struct {
	double xfa; // position
	double v; // velocity limit
	double a; // acceleration limit
	double d; // dwell time (s)
} seg;

//ThreadResource
typedef struct {
	NiFpga_IrqContext irqContext; // context
	table *a_table; // table
	seg *profile; // profile
	int nseg; // no. of segs
	NiFpga_Bool irqThreadRdy; // ready flag
} ThreadResource;

struct biquad{                                      // this should be defined before using.
	double b0; double b1; double b2; // numerator
	double a0; double a1; double a2; // denominator
	double x0; double x1; double x2; // input
	double y1; double y2;            // output
};

#include "myPIDF.h"

/* prototypes */
double cascade(double xin,        // input
			   struct biquad *fa, // biquad array
			   int ns,            // no. segments
			   double ymin,       // min output
			   double ymax);      // max output

void *Timer_Irq_Thread(void* resource);

void Aio_Write(MyRio_Aio* channel, double value);// send output value to analog output.

NiFpga_Status EncoderC_initialize(NiFpga_Session myrio_session, MyRio_Encoder *channel);

int Sramps( seg *segs, // segments array
			int *iseg, // current segment index
			int nseg, // number of segments
			int *itime, // current time index
			double T, // sample period
			double *xa); // next reference position

double pos(void);

/* definitions */

// return the current input reference position pref
int  Sramps(seg *segs, int *iseg, int nseg, int *itime, double T, double *xa)
{
    // Computes the next position, *xa, of a uniform sampled position profile.
    // The profile is composed of an array of segments (type: seg)
    // Each segment consists of:
    //      xfa:    final position
    //      v:      maximum velocity
    //      a:      maximum acceleration
    //      d:      dwell time at the final position
    //  Called from a loop, the profile proceeds from the current position,
    //  through each segment in turn, and then repeats.
    // Inputs:
    //  seg *segs:  - segments array
    //  int *iseg:  - variable hold segment index
    //  int nseg:   - number of segments in the profile
    //  int *itime  - time index within a segment (= -1 at segment beginning)
    //  double T:   - time increment
    // Outputs:
    //  double *xa: - next position in profile
    // Returns:     n - number of samples in the profile, 0 otherwise
	//
	//  Call with *itime = -1, *iseg = -1, outside the loop to initialize.

    double t, t1=0, t2=1, tf=1, tramp, x1=1, xramp, xfr=1, xr, d;
    static double x0, dir;
    static int ntot;
    double vmax=1, amax=1;
    int n;

    if (*itime==-1) {
        (*iseg)++;
        if(*iseg==nseg) {
        	*iseg=0;
        	ntot = 0;
        }
        *itime=0;
        x0=*xa;
    }
    vmax=segs[*iseg].v;
    amax=segs[*iseg].a;
    d=segs[*iseg].d;
    xfr=segs[*iseg].xfa-x0;
    dir=1.0;
    if(xfr<0){
        dir=-1.;
        xfr=-xfr;
    }
    t1 = vmax/amax;
    x1 = 1./2.*amax*t1*t1;
    if (x1<xfr/2) {
        xramp = xfr-2.*x1;
        tramp = xramp/vmax;
        t2 = t1+tramp;
        tf = t2+t1;
    } else {
        x1 = xfr/2;
        t1 = sqrt(2*x1/amax);
        t2 = t1;
        tf = 2.*t1;
    }
    n = trunc((tf+d)/T)+1;

    t = *itime*T;
    if(t<t1) {
        xr = 1./2.*amax*t*t;
    } else if (t>=t1 && t<t2) {
        xr = x1+vmax*(t-t1);
    } else if (t>=t2 && t<tf) {
        xr = xfr-1./2.*amax*(tf-t)*(tf-t);
    } else {
        xr = xfr;
    }
    *xa=x0+dir*xr;
    (*itime)++;
    if(*itime==n+1) {
    	ntot = ntot + *itime - 1;
        *itime=-1;
        if(*iseg==nseg-1) {
        	return ntot;
        }
    }
    return 0;
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

// read the encoder counter and return the displacement in unit BDI
double pos(void){
	static int cn, first_cn, counter = 1; // cn for the current counter, cn_1 for the previous one.
	// need to set the counter as static so that it won't keep changing when I called it.
	double count;
	if (counter == 1){ // when encoder counter is first called, set the current one as previous one.
		cn = Encoder_Counter(&encC0);
		first_cn = cn;
		counter++;
	}else{
		cn = Encoder_Counter(&encC0); // read the value again.
	}
	count = cn - first_cn; // displacement relative to the first position read.
	return count;
}

void *Timer_Irq_Thread(void* resource){

	// cast its input argument into appropriate form.
	ThreadResource* threadResource = (ThreadResource*) resource;

	//	// declare names for the table entries from the table pointer
	//	// and for the ramp segment variables.
	double *pref = &((threadResource->a_table+0)->value);
	double *pact = &((threadResource->a_table+1)->value);
	double *VDAmV = &((threadResource->a_table+2)->value);
	seg *mySegs = threadResource->profile;
	int nseg = threadResource->nseg;

	extern NiFpga_Session myrio_session;

	// Define variable type double of input, output, min and max values
	double T = 0.005, BTI = 0.005, ymin = -7.5, ymax = 7.5;
	double Pact, error, vout, torq;
	int itime = -1, iseg = -1;

    AIO_initialize(&CI0, &CO0);					// Initialize the analog input/output
    Aio_Write(&CO0, 0.0);						// Set the analog output to 0V

    EncoderC_initialize( myrio_session, &encC0); // Set up the encoder counter interface

	while (threadResource->irqThreadRdy == NiFpga_True){
		uint32_t irqAssert = 0;

		// wait IRQ to assert.
		Irq_Wait( threadResource->irqContext,
				  TIMERIRQNO,
				  &irqAssert,
				  (NiFpga_Bool*) &(threadResource->irqThreadRdy)); // Returns irqReady flag set in main

		 NiFpga_WriteU32( myrio_session,
							   IRQTIMERWRITE,
							   timeoutValue );

		 NiFpga_WriteBool( myrio_session,
								IRQTIMERSETTIME,
								NiFpga_True);

		 if(irqAssert) {

			 // Call Sramps() to compute the value of the current reference position
			 Sramps(mySegs, &iseg, nseg, &itime, T, pref);

			 // Call pos() to obtain current position
			 Pact = pos()/2000.0; // unit: rev
			 *pact = Pact; // could not save it as the same name

			 // Compute the error in unit rad
			 error = (*pref - *pact)*2*PI;

			 // Call cascade() to compute the current error
			 vout = cascade(error, PIDF, PIDF_ns,ymin, ymax );
			 torq  = vout*Kvi*Kt;


			 // write the control value to the channel
			 Aio_Write(&CO0,vout);

			 // Change the show values in the table
			 *VDAmV = vout * 1000.0; // Units in mV

			 // Store the output data in the buffer
			 if(pref_pt < (pref_buffer + IMAX))  *pref_pt++ = *pref*2*PI;
			 if (pact_pt < (pact_buffer + IMAX)) *pact_pt++ = Pact*2*PI;
			 if (torq_pt < (torq_buffer + IMAX)) *torq_pt++ = torq;

			 // Acknowledge the interrupt
			 Irq_Acknowledge(irqAssert);
		 }

	}

	int err;
		MATFILE *mf;
		mf= openmatfile("Lab8_JL.mat", &err);						//initialize connection to create MATLAB file
		if(!mf)	printf("Can't open mat file %d\n", err );
		matfile_addstring(mf,"name","Jeffrey Lee");			//create string in the matlab file (ie name)
		matfile_addmatrix(mf,"pact",pact_buffer, IMAX,1,0);
		matfile_addmatrix(mf,"pref",pref_buffer, IMAX,1,0);
		matfile_addmatrix(mf,"torque",torq_buffer, IMAX,1,0);
		matfile_addmatrix(mf,"PIDF",(double *)PIDF, 6,1,0);
		matfile_addmatrix(mf,"BTI",&BTI, 1,1,0);

		matfile_close(mf);

		Aio_Write(&CO0, 0.0);

	pthread_exit(NULL);					// Terminate the thread
	return NULL;						// Return NULL

}

int main(int argc, char **argv)
{
	NiFpga_Status status;

    status = MyRio_Open();		    /*Open the myRIO NiFpga Session.*/
    if (MyRio_IsNotSuccess(status)) return status;

    //my code here
    char *TableTitle = "Pos Control Table";
    int nval = 3;
    MyRio_IrqTimer irqTimer0;
    ThreadResource irqThread0;
    pthread_t thread;

    // initialize the table editor variables.
    table my_table[] = {
    		{"P_ref: (revs)", 0, 0.0},
    		{"P_act: (revs)", 0, 0.0},
    		{"VDAout: (mV)", 0, 0.0},
    };
    irqThread0.a_table = my_table;

	// initialize an array mySegs[]
    // initialize the path profile variables.
	double vmax = 50.; // revs/s
	double amax = 20.; // revs/s^2
	double dwell = 1.0; // s
	seg mySegs[8] = { // revs
					{10.125, vmax, amax, dwell},
					{20.250, vmax, amax, dwell},
					{30.375, vmax, amax, dwell},
					{40.500, vmax, amax, dwell},
					{30.625, vmax, amax, dwell},
					{20.750, vmax, amax, dwell},
					{10.875, vmax, amax, dwell},
					{ 0.000, vmax, amax, dwell}
	};
	irqThread0.nseg = 8;
	irqThread0.profile = mySegs;


    // set up and enable the timer IRQ interrupt.
	// Registers corresponding to the IRQ channel
	irqTimer0.timerWrite = IRQTIMERWRITE;
	irqTimer0.timerSet = IRQTIMERSETTIME;

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

	// when table exits, signal the timer thread to terminate.
	// wait for the DEL key to terminate the program.


	// set the flag to ThreadResource to terminate the new thread.
	irqThread0.irqThreadRdy = NiFpga_False;
	pthread_join(thread,NULL);

	// Unregister Irq
	Irq_UnregisterTimerIrq(&irqTimer0,
						    irqThread0.irqContext);

	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}

