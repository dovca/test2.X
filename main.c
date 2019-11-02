/* 
 * File:   main.c
 * Author: Sheep
 *
 * Created on July 12, 2018, 7:06 PM
 */

#include "config.h"
#include <xc.h>
#include <stdio.h>
#include <stdlib.h>

const char color_table[] = {0x10, 0x22, 0xFF, 0x10, 0x22, 0xFF, 0x10, 0x22, 0xFF};

//Timer2 period = 19 for 2.5us bit time
const unsigned char timer2_period = 19;

volatile char complete = 0;

int main(int argc, char** argv) {
    //Clear Port A output
    PORTA = 0;
    LATA = 0;
    ANSELA = 0;
    
    //Port A is digital output, A2 is input
    TRISA = 4; 
    
    //Enable DMA1 source count interrupt
    PIE2bits.DMA1SCNTIE = 1;
    
    
    //LED and Frequency tester
    /*while(1) {
        RA0 = 1;
        __delay_ms(500);
        RA0 = 0;
        __delay_ms(500);
    }*/
    
    const int message_size = 3;
    
    //Grant memory access to DMA1 and set it to highest priority
    
    //DMA1 has the highest priority (lowest number, s3.1 p28)
    DMA1PR = 0; 
    //ISR priority must be higher than Main priority (s3.2 p29 Note)
    ISRPR = 1; 
    //Main program has the lowest priority
    MAINPR = 2;
    //Special sequence to allow changes to PRLOCKED(Example 3-1 p29)
    PRLOCK = 0x55; 
    PRLOCK = 0xAA;
    //Lock the priorities in PRLOCK
    PRLOCKbits.PRLOCKED = 1;
    
    //INIT Timer2
    
    //Clock is Fosc/4
    T2CLK = 0x01;
    //Free running mode, reset on T2PR compare
    T2HLT = 0x00;
    //Reset timer
    T2TMR = 0;
    //Timer2 period = 19 for 2.5us bit time
    T2PR = timer2_period;
    //Start timer, no pre-scaler, no post-scaler
    T2CONbits.OUTPS = 0;
    T2CONbits.CKPS = 0;
    T2CONbits.ON = 0x80;
    
    //INIT PWM5
    //(s24.1.10, p357)
    
    //Calculate PMW duty cycle as 20% of (T2PR * 2)
    const unsigned short pwm_duty_cycle = (timer2_period * 2) / 5;
    
    //Clear PWMxCON
    PWM5CON = 0;
    //PWM5 clock source is Timer2 (p359)
    CCPTMRS1bits.P5TSEL = 0x01;
    //Set PWM5 duty cycle
    //8 MSBs are in PWM5DCH, 2 LSBs are in highest bits of PWM5DCL
    //This means, we have to shift the value by 6 bits that are unused in
    //PWM5DCL
    PWM5DC = pwm_duty_cycle << _PWM5DCL_DC_POSITION;
    //Wait until Timer2 overflows
    while (!TMR2IF);
    //Enable PWM5
    PWM5CON = 0x80;
    
    //INIT SPI
    
    //Interrupt on transfer finish
    SPI1INTEbits.SRMTIE = 1;
    //Transfer 3 bytes
    SPI1TCNT = message_size;
    //Send 8 bits at a time
    SPI1TWIDTH = 0x00;
    //2x frequency divider (Equation 32-1, p525)
    SPI1BAUD = 0x00;
    //Enable SPI, MSB first, master, BMODE = 1
    SPI1CON0 = 0x83;
    //Transmit only mode
    SPI1CON2 = 0x02;
    //SPI clock source = Timer2
    SPI1CLK = 0x05;
    
    //INIT CLC
    
    //Enable CLC1, logic function = OR-XOR
    CLC1CON = 0x81;
    //Do not output inverted signal
    CLC1POL = 0x00;     
    //input0 = SCK, input1 = SDO, input2 = PWM5, input3 = undefined
    CLC1SEL0 = 44;
    CLC1SEL1 = 43;
    CLC1SEL2 = 24;
    //Gate0: SCK & nSDO & PWM5
    CLC1GLS0 = 0x26;
    //Gate1: SCK & SDO
    CLC1GLS1 = 0x0A;
    //Gate2 and Gate3: no input
    CLC1GLS2 = CLC1GLS3 = 0x00;
    //Output CLC1 on pin RA1
    RA1PPS = 0x01;
    
    //INIT DMA
    
    //DMA1 source memory region is GPR/SFR
    DMA1CON1bits.SMR = 0; 
    //DMA1 source pointer increments after each transfer
    DMA1CON1bits.SMODE = 1; 
    //Clear DMA1 SIRQEN when source counter reloads
    //DMA1CON1bits.SSTP = 1; 
    //Source size
    DMA1SSZ = message_size;
    //Source address
    DMA1SSA = (volatile __int24) color_table;
    //Destination size - 1 byte
    DMA1DSZ = 1;
    //Destination address - SPI TX buffer
    DMA1DSA = (volatile unsigned short) &SPI1TXB;
    //Trigger DMA start on SPI transfer complete
    DMA1SIRQ = 21;
    
    //Enable global interrupts
    ei();
    
    //Enable and start DMA
    DMA1CON0 = 0xC0;
    
    while(1);
    
    return (EXIT_SUCCESS);
}

void __interrupt(irq(16)) DMA1SCNT_ISR() {
    complete = 1;
    DMA1CON0 = 0;
    /*PIR2bits.DMA1SCNTIF = 0;
    T2CONbits.T2ON = 0; //Stop Timer2
    SPI1CON0bits.EN = 0;
    PORTAbits.RA0 = 1;*/
}

