#define __AVR_ATtiny13__
#include <avr/io.h>
#include <avr/interrupt.h>

#define ADC_MAX 1024
#define WATER_PUPM_PIN 3


void configuration()
{
    cli();
    DDRB = (1<<PIN0)|(1<<PIN1)|(1<<PIN3);
    //Pull-up for button NEED CORRECT FUSE
    PORTB = (1<<PORTB5);

    WDTCR = WDCE<<1;
    //INT each 8s
    WDTCR |= (1<<WDP3)|(0<<WDP2)|(0<<WDP1)|(1<<WDP0);
    //Enable WD interrupt
    WDTCR |= (1<<WDTIE);

    //ADC internal reference, right adjust PB2 (soil sensor)
    ADMUX = (1<<REFS0)|(0<<ADLAR)|(0<<MUX1)|(1<<MUX0);
    //Enable ADC
    ADCSRA |= (1<<ADEN);
    ADCSRA &= ~(1<<ADIF);
}

uint16_t getADCResult(uint8_t mask)
{
    ADMUX ^= mask;
    //Start convertion
    ADCSRA |= (1<<ADSC);
    while (!(ADCSRA & 1<<ADIF)); //Wait util conversion complete
    ADCSRA &= ~(1<<ADIF);
    uint16_t data = 0;
    data = (ADCH<<8)|ADCL;
    return data;
}


int main()
{
    configuration();
    while(1)
    {
        
    }
    return 0;
}