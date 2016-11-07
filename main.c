// main.c
// Runs on LM4F120/TM4C123
// UART runs at 115,200 baud rate 
// Daniel Valvano
// May 3, 2015

/* This example accompanies the books
  "Embedded Systems: Introduction to ARM Cortex M Microcontrollers",
  ISBN: 978-1469998749, Jonathan Valvano, copyright (c) 2015

"Embedded Systems: Real Time Interfacing to ARM Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2015
 
 Copyright 2015 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */


#include <stdint.h> // C99 variable types
#include "../inc/tm4c123gh6pm.h"
#include "ADCSWTrigger.h"
#include "uart.h"
#include "FIFO.h"
#include "PLL.h"
#include "data.h"
#include "ST7735.h"

long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void EnableInterrupts(void);
void DisableInterrupts(void);

void Delay1ms(uint32_t);
void Timer0A_Init(uint32_t period);
void printTemp(int32_t i);

int numSamples;

int main(void){
  DisableInterrupts();
  PLL_Init(Bus80MHz);   // 80 MHz
  UART_Init();              // initialize UART device
  ST7735_InitR(INITR_REDTAB);
  ST7735_DrawString(0,0,"EE445L Thermometer",ST7735_YELLOW);
  ST7735_DrawString(0,2,"UT Austin-S Ma, E Su",ST7735_YELLOW);
  ST7735_DrawString(0,1,"T= ",ST7735_YELLOW);
  ST7735_FillRect(0,32,128,160, ST7735_WHITE);
  ADC0_InitSWTriggerSeq3_Ch9();  
  Timer0A_Init(80000);
  TxFifo_Init();
  Delay1ms(500);
  EnableInterrupts();
  int xd = 0;
  int cnt = 0;
  int32_t tmp[1];
  while(1){
    
      TxFifo_Get(tmp);
      //cnt ++;
      //xd += tmp[0];
      //if (cnt == 20) {
        printTemp(tmp[0]);
        //xd = 0;
        //cnt = 0;
      //}
      //printTemp(2250);
      //Delay1ms(100);
  }
}

void printTemp(int32_t i) {
    int index = 0;
    double slope = 0.0;
    while (i > ADCdata[index]) {
        index++;
    }
    slope = ((double)(Tdata[index]-Tdata[index-1]))/((double)(ADCdata[index]-ADCdata[index-1]));
    double b = Tdata[index] - slope*ADCdata[index];
    int fixpt = (int)(slope*i)+ b;
    char str[6] = "00.0 C";
    int32_t divisor = 1000;
    if (fixpt > 1000) {
      str[0] = fixpt/divisor+48;
    }
    fixpt %= divisor;
    divisor /= 10;
    str[1] = fixpt/divisor+48;
    fixpt %= divisor;
    divisor /= 10;
    str[3] = fixpt/divisor+48;
    ST7735_DrawString(3,1,str,ST7735_YELLOW);
}

// ***************** Timer0A_Init ****************
// Activate TIMER0 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq), 32 bits
// Outputs: none
void Timer0A_Init(uint32_t period){
    long sr;
    numSamples = 0;
    sr = StartCritical(); 
    SYSCTL_RCGCTIMER_R |= 0x01;   // 0) activate TIMER0
    TIMER0_CTL_R = 0x00000000;    // 1) disable TIMER0A during setup
    TIMER0_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
    TIMER0_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
    TIMER0_TAILR_R = period-1;    // 4) reload value
    TIMER0_TAPR_R = 0;            // 5) bus clock resolution
    TIMER0_ICR_R = 0x00000001;    // 6) clear TIMER0A timeout flag
    TIMER0_IMR_R = 0x00000001;    // 7) arm timeout interrupt
    NVIC_PRI4_R = (NVIC_PRI4_R&0x00FFFFFF)|0xA0000000; // 8) priority 4
    // interrupts enabled in the main program after all devices initialized
    // vector number 35, interrupt number 19
    NVIC_EN0_R = 1<<19;           // 9) enable IRQ 19 in NVIC
    TIMER0_CTL_R = 0x00000001;    // 10) enable TIMER0A
    EndCritical(sr);
}

void Timer0A_Handler(void) {
    TIMER0_ICR_R = TIMER_ICR_TATOCINT;
    int32_t data = ADC0_InSeq3();
    // Output to FIFO
    TxFifo_Put(data);
}

