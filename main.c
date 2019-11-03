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

const char color_table[] = {
    0x00, 0x00, 0xFF,
    0x00, 0xFF, 0x00,
    0xFF, 0x00, 0x00,
    0x00, 0xFF, 0xFF
};

//Timer2 period = 19 for 2.5us bit time
const unsigned char timer2_period = 19;

volatile char complete = 0;

int main(int argc, char** argv) {
    //Clear Port A output
    PORTA = 0;
    LATA = 0;
    ANSELA = 0;
    //All PORTA pins are digital outputs
    TRISA = 0;
    
    PORTAbits.RA0 = 1;
    __delay_ms(500);
    
    //Enable DMA1 source count interrupt
    PIE2bits.DMA1SCNTIE = 1;
    
    const int message_size = 12;
    
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
    T2CONbits.ON = 1;
    
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
    //Enable PWM5
    PWM5CONbits.EN = 1;
    
    //INIT SPI
    
    //Interrupt on transfer finish
    SPI1INTEbits.SRMTIE = 1;
    //Transfer 3 bytes
    SPI1TCNT = message_size;
    //Send 8 bits at a time
    SPI1TWIDTHbits.TWIDTH = 0;
    //2x frequency divider (Equation 32-1, p525)
    SPI1BAUD = 0x00;
    //BMODE = 1, TWIDTH means number of bytes, not bits
    SPI1CON0bits.BMODE = 1;
    //SPI Master mode
    SPI1CON0bits.MST = 1;
    //MSB first
    SPI1CON0bits.LSBF = 0;
    //Transmit only mode
    SPI1CON2bits.TXR = 1;
    SPI1CON2bits.RXR = 0;
    //SPI clock source = Timer2
    SPI1CLKbits.CLKSEL = 0x05;
    //Enable SPI1
    SPI1CON0bits.EN = 1;
    
    //INIT CLC
    //Details in http://ww1.microchip.com/downloads/en/AppNotes/00001606A.pdf
    
    //Reset polarity
    CLC1POL = 0x00;     
    //input1 = SCK, input2 = SDO, input3 = PWM5, input4 = undefined
    CLC1SEL0bits.D1S = 44;
    CLC1SEL1bits.D2S = 43;
    CLC1SEL2bits.D3S = 24;
    //Gate0: SCK & nSDO & PWM5 = n(nSCK | SDO | nPWM5)
    CLC1GLS0bits.G1D1N = 1;
    CLC1GLS0bits.G1D1T = 0;
    CLC1GLS0bits.G1D2N = 0;
    CLC1GLS0bits.G1D2T = 1;
    CLC1GLS0bits.G1D3N = 1;
    CLC1GLS0bits.G1D3T = 0;
    CLC1GLS0bits.G1D4N = 0;
    CLC1GLS0bits.G1D4T = 0;
    //Invert polarity, we want to AND the inputs
    CLC1POLbits.G1POL = 1;
    //Gate1: SCK & SDO = n(nSCK | nSDO)
    CLC1GLS1bits.G2D1N = 1;
    CLC1GLS1bits.G2D1T = 0;
    CLC1GLS1bits.G2D2N = 1;
    CLC1GLS1bits.G2D2T = 0;
    CLC1GLS1bits.G2D3N = 0;
    CLC1GLS1bits.G2D3T = 0;
    CLC1GLS1bits.G2D4N = 0;
    CLC1GLS1bits.G2D4T = 0;
    //Invert polarity, we want to AND the inputs
    CLC1POLbits.G2POL = 1;
    //Gate2 and Gate3: no input
    CLC1GLS2 = CLC1GLS3 = 0x00;
    //OR-XOR mode (p439)
    CLC1CONbits.MODE = 0b001;
    //Output CLC1 on pin RA1
    RA1PPS = 0x01;
    //Enable CLC1
    CLC1CONbits.EN = 1;
    
    //INIT DMA
    
    //DMA1 source memory region is PFM (really?)
    DMA1CON1bits.SMR = 0b01; 
    //DMA1 source pointer increments after each transfer
    DMA1CON1bits.SMODE = 1;
    //DMA1 destination pointer remains unchanged
    DMA1CON1bits.DMODE = 0;
    //Stop transfers when source counter reloads (s15.5.2.3, p234)
    DMA1CON1bits.SSTP = 1; 
    //Source size: number of bytes to transfer
    DMA1SSZ = message_size;
    //Source address
    DMA1SSA = (volatile __int24) color_table;
    //Destination size = 1 byte
    DMA1DSZ = 1;
    //Destination address - SPI TX buffer
    DMA1DSA = (volatile unsigned short) &SPI1TXB;
    //Trigger DMA start on SPI transfer complete
    DMA1SIRQ = 21;
    
    //Enable transfer start on hardware interrupt request
    DMA1CON0bits.SIRQEN = 1;
    //Enable DMA1
    DMA1CON0bits.EN = 1;
    //Start DMA transfer
    DMA1CON0bits.DGO = 1;
    
    //Enable global interrupts
    ei();
    
    while(1);
    
    return (EXIT_SUCCESS);
}

void __interrupt(irq(16)) DMA1SCNT_ISR() {
    complete = 1;
    PORTAbits.RA2 = 1;
    PIR2bits.DMA1SCNTIF = 0;
    
    /*T2CONbits.T2ON = 0; //Stop Timer2
    SPI1CON0bits.EN = 0;
    PORTAbits.RA0 = 1;*/
}

