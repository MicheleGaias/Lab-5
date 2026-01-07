#include <stdio.h>
#include "xstatus.h"
#include "platform.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xuartlite_l.h"
#include "xparameters.h"
#include "xtmrctr_l.h"

#ifndef SDT
#define TMRCTR_BASEADDR		XPAR_TMRCTR_0_BASEADDR
#else
#define TMRCTR_BASEADDR		XPAR_XTMRCTR_0_BASEADDR
#endif

#define TIMER_COUNTER_0	 0
#define UART_BASEADDR XPAR_UARTLITE_0_BASEADDR

#define UART_MODE (u8)1
#define BUTTON_MODE (u8)0

//Led RGB
volatile int * gpio_rgb_data = (volatile int *) (0x40000008);
volatile int * gpio_rgb_tri  = (volatile int *) (0x4000000C);

//Bottone onboard
volatile int * btn_data = (volatile int *) 0x40060000;
volatile int * btn_tri  = (volatile int *) (0x40060000 + 0x4);

volatile int * IER = (volatile int*)	0x41200008;
volatile int * MER = (volatile int*)	0x4120001C;
volatile int * IISR = (volatile int*)	0x41200000;
volatile int * IIAR = (volatile int*)	0x4120000C;

//Variabili globali rgb
volatile u8 R = 0;
volatile u8 G = 0;
volatile u8 B = 0;
volatile int pwm_count = 0;

typedef enum {pressed, idle} debounce_state_t;

// Prototopi funzioni
void myISR(void) __attribute__((interrupt_handler));
int TmrCtrLowLevelExample(UINTPTR TmrCtrBaseAddress, u8 TmrCtrNumber);
int FSM_debounce(int buttons);
void update_leds(u32 data, u8 mode);
u32 my_XUartLite_RecvByte(UINTPTR BaseAddress);


int main()
{
	int Status;
	init_platform();
	// Configura I/O
	*gpio_rgb_tri = 0x00000000; // Output
	*btn_tri = 0xFFFFFFFF;      // Input

	u32 uart_val;
	int btn_clean;
	int btn_raw;

	//Configura timer
	Status = TmrCtrLowLevelExample(TMRCTR_BASEADDR, TIMER_COUNTER_0);
	if (Status != XST_SUCCESS) {
		xil_printf("Timer Failed\r\n");
		return XST_FAILURE;
	}
	//Configura interrupt
	 *IER = XPAR_AXI_TIMER_0_INTERRUPT_MASK;
	 *MER = 0x3;
	 microblaze_enable_interrupts();


	 print("UART RGB Control Ready \n\r");

	 while(1){
		 // 1. Lettura Bottone
		 btn_raw = *btn_data & 0x1;
		 btn_clean = FSM_debounce(btn_raw);

		 if(btn_clean == 1)
		 {
			 // Passa BUTTON_MODE
			 update_leds(200, BUTTON_MODE);
		 }

		 // 2. Lettura UART
		 uart_val = my_XUartLite_RecvByte(UART_BASEADDR);

		 // Aggiorna solo se hai ricevuto un dato valido
		 if (uart_val != 200) {
			 update_leds(uart_val, UART_MODE); // Passa UART_MODE (1)
		 }

	 }

	 cleanup_platform();
	 return 0;
}

u32 my_XUartLite_RecvByte(UINTPTR BaseAddress)
{
// insert your implementation of the non-blocking read here;
	if(XUartLite_IsReceiveEmpty(BaseAddress)){
		return (u8) 200;
	}
	u8 data = (u8)XUartLite_ReadReg(BaseAddress,XUL_RX_FIFO_OFFSET);

	if (data == '\r' || data == '\n') {
			return 200;
	}
	return (u32)data;
}

void update_leds(u32 data, u8 mode){

// insert your implementation of the led update here;
	//Rotazione R -> G -> B
	static int rotazione = 0; //Indice di rotazione dei colori
	if (mode == BUTTON_MODE){
		rotazione ++;
		if(rotazione > 2) rotazione=0;
		if(rotazione==0){
			R=0; G=255; B=255;
		}else if(rotazione==1){
			R=255; G=0; B=255;
		}else if(rotazione==2){
			R=255; G=255; B=0;
		}
        xil_printf("Button pressed. Color index: %d\r\n", rotazione);
	}else if (mode == UART_MODE){
		switch(data){
			case '0': R=255; G=255; B=255; break; //Spento
			case '1': R=255; G=0; B=0; break;
			case '2': R=0; G=255; B=0; break;
			case '3': R=0; G=0; B=255; break;
			case '4': R=255; G=255; B=0; break;
			case '5': R=0; G=255; B=255; break;
			case '6': R=255; G=0; B=255; break;
			case '7': R=255; G=128; B=0; break;
			case '8': R=128; G=0; B=255; break;
			case '9': R=0; G=0; B=0; break; //Tutte accese
		}
        xil_printf("UART Command: %c\r\n", (char)data);
	}
}

//Debounce del pulsante
int FSM_debounce(int buttons){
   	static int debounced_buttons;
   	static debounce_state_t currentState = idle;
   	switch (currentState) {
   		case idle:
   			debounced_buttons=buttons;
   			if (buttons!=0)
   				currentState=pressed;
   			break;
   		case pressed:
   			debounced_buttons=0;
   			if (buttons==0)
   				currentState=idle;
   			break;
   	}
return debounced_buttons;
}

// Funzione del timer
int TmrCtrLowLevelExample(UINTPTR TmrCtrBaseAddress, u8 TmrCtrNumber)
{
    XTmrCtr_SetControlStatusReg(TmrCtrBaseAddress, TmrCtrNumber, 0);
    XTmrCtr_SetLoadReg(TmrCtrBaseAddress, TmrCtrNumber, 2000);
    XTmrCtr_LoadTimerCounterReg(TmrCtrBaseAddress, TmrCtrNumber);
    u32 ControlStatus = XTC_CSR_AUTO_RELOAD_MASK | XTC_CSR_ENABLE_INT_MASK | XTC_CSR_DOWN_COUNT_MASK;
    XTmrCtr_SetControlStatusReg(TmrCtrBaseAddress, TmrCtrNumber, ControlStatus);
    XTmrCtr_Enable(TmrCtrBaseAddress, TmrCtrNumber);

    return XST_SUCCESS;
}

void myISR(void)
{
   unsigned p = *IISR;  // snapshot
   if (p & XPAR_AXI_TIMER_0_INTERRUPT_MASK) {
       pwm_count++;
       u8 count_byte = (u8)pwm_count;
       int rgb_out = 0;
       // Gestione PWM

       if (count_byte < B) rgb_out |= 0x1;
       if (count_byte < G) rgb_out |= 0x2;
       if (count_byte < R) rgb_out |= 0x4;

       *gpio_rgb_data = rgb_out;
       u32 ControlStatus = XTmrCtr_GetControlStatusReg(TMRCTR_BASEADDR,0);
       XTmrCtr_SetControlStatusReg(TMRCTR_BASEADDR, 0, ControlStatus | XTC_CSR_INT_OCCURED_MASK);
       *IIAR  = XPAR_AXI_TIMER_0_INTERRUPT_MASK;


   }
}
