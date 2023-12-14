#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr.h"
#include "xintc.h"
#include "xgpio.h"
#include "xmutex.h"

// shared DDR2
#define DDR2_SIZE     	0x8000000
#define DDR2_BASE_ADDRESS 0x80000000
#define DDR2_LAST_ADDRESS 0x87FFFFFF
#define DDR2_10_MB_SIZE   0x8000 // 0xA00000

// shared BRAM
#define BRAM_SIZE     	0x40000
#define BRAM_BASE_ADDRESS 0xC0000000
#define BRAM_LAST_ADDRESS 0xC003FFFF

// useful defines
#define UART_MUTEX 	15
#define BARRIER_MUTEX  14
#define BARRIER_1_ADDR 0xC0000000
#define BARRIER_2_ADDR 0xC0000004

// peripherals
XTmrCtr timer_0;
XIntc   intc_0;
XGpio   gpio_16led_16switch;
XMutex  mutex_0;

// global variables
int timer_0_isr_flag = 0;
int passed_s     	= 0;

int gpio_16led  	= 0x5555; //active high
int gpio_16switches = 0;;

// interrupt service routines
void timer_0_isr() {
    timer_0_isr_flag = 1;
    passed_s++;
    XTmrCtr_Reset(&timer_0, 0);
    XTmrCtr_Start(&timer_0, 0);
}

void sort(int arr[], int l, int r);

void merge(int arr[], int l, int m, int r);


int main() {
    *(int *)BARRIER_1_ADDR = 0;
    *(int *)BARRIER_2_ADDR = 0;

    int i;
    // configure and initialize peripherals --------------------------------------------------
    XTmrCtr_Config tmr_config = {
   		 .DeviceId   	= XPAR_AXI_TIMER_0_DEVICE_ID,
   		 .BaseAddress	= XPAR_AXI_TIMER_0_BASEADDR,
   		 .SysClockFreqHz = XPAR_AXI_TIMER_0_CLOCK_FREQ_HZ };
    XTmrCtr_CfgInitialize(&timer_0,
   		 &tmr_config,
   		 XPAR_AXI_TIMER_0_BASEADDR);

    XGpio_Config gpio_led_sw_config = {
   		 .DeviceId     	= XPAR_AXI_GPIO_1_DEVICE_ID,
   		 .BaseAddress  	= XPAR_AXI_GPIO_1_BASEADDR,
   		 .InterruptPresent = XPAR_AXI_GPIO_1_INTERRUPT_PRESENT,
   		 .IsDual       	= XPAR_AXI_GPIO_1_IS_DUAL };
    XGpio_CfgInitialize(&gpio_16led_16switch,
   		 &gpio_led_sw_config,
   		 XPAR_AXI_GPIO_1_BASEADDR);

    XMutex_Config my_mux_cfg = {
   		 .DeviceId	= XPAR_MUTEX_0_IF_0_DEVICE_ID,
   		 .BaseAddress = XPAR_MUTEX_0_IF_0_BASEADDR,
   		 .NumMutex	= XPAR_MUTEX_0_IF_0_NUM_MUTEX,
   		 .UserReg 	= XPAR_MUTEX_0_IF_0_ENABLE_USER};
    XMutex_CfgInitialize(&mutex_0,
   		 &my_mux_cfg,
   		 XPAR_MUTEX_0_IF_0_BASEADDR);

    XIntc_Initialize(&intc_0, XPAR_AXI_INTC_0_DEVICE_ID);
    XIntc_Connect(&intc_0,
   			   XPAR_INTC_0_TMRCTR_0_VEC_ID,
   			   (XInterruptHandler)timer_0_isr,
   			   (void *)0);
    XIntc_Start(&intc_0, XIN_REAL_MODE);
    XIntc_Enable(&intc_0,
   		 XPAR_INTC_0_TMRCTR_0_VEC_ID);
    // ---------------------------------------------------------------------------------------

    // Register the interrupt controller handler with the exception table.
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
   						      (Xil_ExceptionHandler)XIntc_InterruptHandler,
   							 &intc_0);
    // Enable exceptions.
    Xil_ExceptionEnable();
    // ---------------------------------------------------------------------------------------

    XTmrCtr_SetOptions(&timer_0, 0, XTC_DOWN_COUNT_OPTION | XTC_INT_MODE_OPTION);
    XTmrCtr_SetResetValue(&timer_0, 0, 100000000); // Start count DOWN a second
    XTmrCtr_Start(&timer_0, 0);

    // Get GPIOs as Input/Output
    XGpio_SetDataDirection(&gpio_16led_16switch, 1, 0);  	// output
    XGpio_SetDataDirection(&gpio_16led_16switch, 2, 0xFFFF); // input
    XGpio_DiscreteWrite(&gpio_16led_16switch, 1, gpio_16led);

    XMutex_Lock(&mutex_0, UART_MUTEX);
    xil_printf("C%d: Peripheral initialization done.\n\r", XPAR_CPU_ID);
    XMutex_Unlock(&mutex_0, UART_MUTEX);
    // ---------------------------------------------------------------------------------------

    XMutex_Lock(&mutex_0, UART_MUTEX);
    xil_printf("C%d: Initialize the DDR2 memory...\n\r", XPAR_CPU_ID);
    XMutex_Unlock(&mutex_0, UART_MUTEX);

    // 1 int = 4 chars
    for(i = 0; i < DDR2_10_MB_SIZE; i = i + 16) {
   	 *(int *)(DDR2_BASE_ADDRESS + i) = XTmrCtr_GetValue(&timer_0, 0);
    }

    XMutex_Lock(&mutex_0, UART_MUTEX);
    xil_printf("C%d: DDR2 memory initialization done.\n\r", XPAR_CPU_ID);
    XMutex_Unlock(&mutex_0, UART_MUTEX);
    // ---------------------------------------------------------------------------------------


    // BARRIER - All CPUs should sync here!
    XMutex_Lock(&mutex_0, BARRIER_MUTEX);
    (*(int *)BARRIER_1_ADDR)++;
    XMutex_Unlock(&mutex_0, BARRIER_MUTEX);
    XMutex_Lock(&mutex_0, UART_MUTEX);
    xil_printf("C%d: Waiting at barrier 1...\n\r", XPAR_CPU_ID);
    XMutex_Unlock(&mutex_0, UART_MUTEX);
    while(*(int *)BARRIER_1_ADDR < 4)
   	 ;
    // ---------------------------------------------------------------------------------------

    XMutex_Lock(&mutex_0, UART_MUTEX);
    xil_printf("C%d: Initialization time: %d s and %d us\n\r", XPAR_CPU_ID,
   		 passed_s,
   		 (100000000 - XTmrCtr_GetValue(&timer_0, 0)) / 100);
    XMutex_Unlock(&mutex_0, UART_MUTEX);
    // !!! Start time
    passed_s = 0; // reset time
    XTmrCtr_Reset(&timer_0, 0);
    XTmrCtr_Start(&timer_0, 0);

    // ***------------------------------------------------------------------------------------
    XMutex_Lock(&mutex_0, UART_MUTEX);
    xil_printf("C%d: Starts sorting...\n\r", XPAR_CPU_ID);
    XMutex_Unlock(&mutex_0, UART_MUTEX);

    // YOUR SORTING ALGORITHM SHOUL BE HERE!
    sort((int *)DDR2_BASE_ADDRESS, 0, DDR2_10_MB_SIZE / 4 / sizeof(int) - 1);



    XMutex_Lock(&mutex_0, UART_MUTEX);
    xil_printf("C%d: Sorting done.\n\r", XPAR_CPU_ID);
    XMutex_Unlock(&mutex_0, UART_MUTEX);
    // ***------------------------------------------------------------------------------------


    // BARRIER - All CPUs should sync here!
    XMutex_Lock(&mutex_0, BARRIER_MUTEX);
    (*(int *)BARRIER_2_ADDR)++;
    XMutex_Unlock(&mutex_0, BARRIER_MUTEX);
    XMutex_Lock(&mutex_0, UART_MUTEX);
    xil_printf("C%d: Waiting at barrier 2...\n\r", XPAR_CPU_ID);
    XMutex_Unlock(&mutex_0, UART_MUTEX);
    while(*(int *)BARRIER_2_ADDR < 4)
   	 ;
    // ---------------------------------------------------------------------------------------

    // !!! Stop time
    XMutex_Lock(&mutex_0, UART_MUTEX);
    xil_printf("C%d: Sort time: %d s and %d us\n\r", XPAR_CPU_ID,
   		 passed_s,
   		 (100000000 - XTmrCtr_GetValue(&timer_0, 0)) / 100);
    XMutex_Unlock(&mutex_0, UART_MUTEX);

    XMutex_Lock(&mutex_0, UART_MUTEX);
    xil_printf("C%d: Check if sort is correct...\n\r", XPAR_CPU_ID);
    XMutex_Unlock(&mutex_0, UART_MUTEX);
    int in_order_flag = 1;
    for(i = 0; i < DDR2_10_MB_SIZE - 4; i = i + 4) {
   	 // XMutex_Lock(&mutex_0, UART_MUTEX);
   	 // xil_printf("C%d: %d\n\r", XPAR_CPU_ID, *(int *)(DDR2_BASE_ADDRESS + i));
   	 // XMutex_Unlock(&mutex_0, UART_MUTEX);
   	 if(*(int *)(DDR2_BASE_ADDRESS + i) > *(int *)(DDR2_BASE_ADDRESS + i + 4)) {
   		 in_order_flag = 0;
   		 XMutex_Lock(&mutex_0, UART_MUTEX);
   		 xil_printf("C%d: Error: String is not ordered correctly\n\r", XPAR_CPU_ID);
   		 XMutex_Unlock(&mutex_0, UART_MUTEX);
   		 break;
   	 }
    }
    if(in_order_flag) {
   	 XMutex_Lock(&mutex_0, UART_MUTEX);
   	 xil_printf("C%d: The order seems fine.\n\r", XPAR_CPU_ID);
   	 xil_printf("C%d: Required time: .\n\r", XPAR_CPU_ID);
   	 XMutex_Unlock(&mutex_0, UART_MUTEX);
    }

    XMutex_Lock(&mutex_0, UART_MUTEX);
    xil_printf("C%d: DONE.\n\r", XPAR_CPU_ID);
    XMutex_Unlock(&mutex_0, UART_MUTEX);

    while(1); // go into infinite loop

    return 0;
}

void merge(int arr[], int l, int m, int r)
{
    int i, j, k;
    int n1 = m - l + 1;
    int n2 = r - m;
 
    int L[n1], R[n2];
 
    for (i = 0; i < n1; i++)
        L[i] = arr[l + i];
    for (j = 0; j < n2; j++)
        R[j] = arr[m + 1 + j];
 
    i = 0;
    j = 0;
    k = l;
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            arr[k] = L[i];
            i++;
        }
        else {
            arr[k] = R[j];
            j++;
        }
        k++;
    }
 
    while (i < n1) {
        arr[k] = L[i];
        i++;
        k++;
    }
 
    while (j < n2) {
        arr[k] = R[j];
        j++;
        k++;
    }
}
 

void sort(int arr[], int l, int r)
{
    if (l < r) {
        int m = l + (r - l) / 2;
 
        sort(arr, l, m);
        sort(arr, m + 1, r);
 
        merge(arr, l, m, r);
    }
}
