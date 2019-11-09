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

char color_table[] = {
    /*
    B     R     G
    */ 
    0xFF, 0x80, 0xFF, //3
    0xA0, 0x80, 0x33, //6
    0xFF, 0xA0, 0x00, //9
    0x0F, 0xDD, 0xE0, //12

    0x00, 0x8F, 0x3F, //15
    0xFF, 0xF0, 0x44, //18
    0x04, 0x4F, 0xEE, //21
    0xC0, 0x00, 0x7F, //24
    
    0x77, 0x05, 0x00, //27
    0xFF, 0x0A, 0x0B, //30
    0xAA, 0x8C, 0x00, //33
    0xFF, 0x88, 0xC8, //36
    
    0xC0, 0xFF, 0x88, //39
    0x22, 0xDF, 0xFF, //42
    0x00, 0x2D, 0x54, //45
    0x58, 0xA3, 0x00, //48
    
    0xBC, 0x79, 0x88, //51
    0xCF, 0x60, 0xFF  //54
};

volatile char message_size = sizeof color_table;

//Timer2 period = 19 for 2.5us bit time
const unsigned char timer2_period = 19;

volatile char dma_transfer_complete = 0;
volatile char spi_transfer_complete = 0;

void reset_ports() {
    //Clear Port A output
    PORTA = 0;
    LATA = 0;
    ANSELA = 0;
    //All PORTA pins are digital outputs
    TRISA = 0;
    
    PORTAbits.RA0 = 1;
    __delay_ms(500);
}

void init_timer2() {
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
}

void init_dma1() {
    //DMA1 source memory region is GPR
    DMA1CON1bits.SMR = 0b00; 
    //DMA1 source pointer increments after each transfer
    DMA1CON1bits.SMODE = 1;
    //DMA1 destination pointer remains unchanged
    DMA1CON1bits.DMODE = 0;
    //Stop transfers when source counter reloads (s15.5.2.3, p234)
    DMA1CON1bits.SSTP = 1; 
    //Source size: number of bytes to transfer
    DMA1SSZ = message_size;
    //Source address
    DMA1SSA = color_table;
    //Destination size = 1 byte
    DMA1DSZ = 1;
    //Destination address - SPI TX buffer
    DMA1DSA = (volatile unsigned short) &SPI1TXB;
    //Trigger DMA start on SPI transfer complete
    DMA1SIRQ = 21;
    //Enable transfer start on hardware interrupt request
    DMA1CON0bits.SIRQEN = 1;
    //Enable DMA1 source count interrupt
    PIE2bits.DMA1SCNTIE = 1;
}

void init_clc1() {
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
}

void init_spi1() {
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
}

void init_pwm5() {
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
}

void start_dma_transfer() {
    //while (!PIR4bits.TMR2IF);
    //Enable transfer start on hardware interrupt request
    DMA1CON0bits.SIRQEN = 1;
    //Enable DMA1
    DMA1CON0bits.EN = 1;
    //Start DMA transfer
    DMA1CON0bits.DGO = 1;
}

int main(int argc, char** argv) {
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
    
    reset_ports();

    init_timer2();
    init_pwm5();
    init_spi1();
    init_clc1();
    
    //Enable global interrupts
    ei();
    
    while(1) {
        dma_transfer_complete = 0;
        
        init_dma1();
        start_dma_transfer();

        while(!dma_transfer_complete);

        for (int i = 0, l = sizeof color_table; i < l; i++) {
            color_table[i] += 3;
        }

        __delay_ms(20);
    }
    
    return (EXIT_SUCCESS);
}

void __interrupt(irq(16)) DMA1SCNT_ISR() {
    dma_transfer_complete = 1;
    
    //Clear output
    PORTAbits.RA1 = 0;
    
    //Disable DMA
    PIR2bits.DMA1SCNTIF = 0;
    DMA1CON0bits.SIRQEN = 0;
    DMA1CON0bits.EN = 0;
    DMA1CON0bits.DGO = 0;
}
