/* Lab #<5> - <Jeffrey Lee> */

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include <string.h>
#include <pthread.h>
#include "DIIRQ.h"
#include "IRQConfigure.h"

/* prototypes */
int32_t Irq_RegisterDiIrq(MyRio_IrqDi* irqChannel,
						  NiFpga_IrqContext* irqContext,
						  uint8_t irqNumber,
						  uint32_t count,
						  Irq_Dio_Type type);

int pthread_create(pthread_t *thread,  // thread: a pointer to a thread identifier.
				   const pthread_attr_t *attr, //attr: a pointer to thread attributes.
				   void *(*start_routine)(void*),
				   void *arg);

int32_t Irq_UnregisterDiIrq(MyRio_IrqDi* irqChannel,
							NiFpga_IrqContext irqContext,
							uint8_t irqNumber);

void *DI_Irq_Thread(void* resource);

void wait(void);

/* definitions */
typedef struct{
	NiFpga_IrqContext irqContext; // IRQ context reserved.
	NiFpga_Bool irqThreadRdy; // IRQ thread ready flag.
	uint8_t irqNumber; // IRQ number value.
}ThreadResource; // globally defined for two threads to communicate

void *DI_Irq_Thread(void* resource){
	// cast its input argument into appropriate form.
	ThreadResource* threadResource = (ThreadResource*) resource;
	uint32_t irqAssert = 0;
	while (threadResource->irqThreadRdy == NiFpga_True){
		// times out after 100 ms.
		Irq_Wait(threadResource->irqContext,
				 threadResource->irqNumber,
				 &irqAssert,
				 (NiFpga_Bool*)&(threadResource->irqThreadRdy));
		if(irqAssert&(1 << threadResource->irqNumber)){
			printf_lcd("interrupt_");
			Irq_Acknowledge(irqAssert);
		}

	}
	pthread_exit(NULL);
	return NULL;
};

void wait(void){
	uint32_t i;
	i = 417000;
	while(i>0){
		i--;
	}
	return;
};

int main(int argc, char **argv)
{
	NiFpga_Status status;

    status = MyRio_Open();		    /*Open the myRIO NiFpga Session.*/
    if (MyRio_IsNotSuccess(status)) return status;

    //my code here
    int32_t irq_status;
    ThreadResource irqThread0;
    pthread_t thread;
    MyRio_IrqDi irqDI0;
    int count = 1,i;

    // I. Configure the DI IRQ
    const uint8_t IrqNumber = 2;
    const uint32_t Count = 1;
    const Irq_Dio_Type TriggerType = Irq_Dio_FallingEdge;

    /* Specify IRQ channel settings */
    irqDI0.dioCount = IRQDIO_A_0CNT;
    irqDI0.dioIrqNumber = IRQDIO_A_0NO;
    irqDI0.dioIrqEnable = IRQDIO_A_70ENA;
    irqDI0.dioIrqRisingEdge = IRQDIO_A_70RISE;
    irqDI0.dioIrqFallingEdge = IRQDIO_A_70FALL;
    irqDI0.dioChannel = Irq_Dio_A0;


    // Initiate the IRQ number resource for new thread.
    irqThread0.irqNumber = IrqNumber;

    //Register DIO IRQ. Terminate if not successful.
    irq_status = Irq_RegisterDiIrq(&irqDI0,
    						   	   &(irqThread0.irqContext),
    						   	   IrqNumber,
    						   	   Count,
    						   	   TriggerType);

    // Set the indicator to allow the new thread.
    irqThread0.irqThreadRdy = NiFpga_True;

    // II. Create new thread to catch the IRQ.
    irq_status = pthread_create(&thread,
    						    NULL,
    						    DI_Irq_Thread,
    						    &irqThread0);
    while(count <= 60){
    	for (i = 1;i <= 200; i++){
    		wait();
    	}
		printf_lcd("\fCounter: %d",count);
		count++;
    };
    irqThread0.irqThreadRdy = NiFpga_False;
    irq_status = pthread_join(thread,NULL);

    irq_status = Irq_UnregisterDiIrq(&irqDI0,
    								 &(irqThread0.irqContext),
    								 IrqNumber);

	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}
