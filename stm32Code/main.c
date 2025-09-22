#include "function.h"
#include <stdio.h>
#include "stm32f4xx.h"
int nbre_car = 50 ;





void EXTI0_IRQHandler(void){
		if (EXTI->PR&(1<<1)){
			open_out(); //open servo motor aout
			delay_ms(10000);
			close_out();
			nbre_car= nbre_car + 1;
			EXTI->PR |= (1 << 0); //effacer l’interruption
		}
}









int main(void){

    RCC->AHB1ENR |= 0b1;          // Enabling port A (bit 0)
    RCC->AHB1ENR |= (1 << 1);     // Enabling port B (bit 1)

	GPIOA->MODER &= ~(0b11 | (0b11 << 2) | (0b11 << 4));   // Clear PA0, PA1, PA2
	GPIOA->MODER |=  (0b01 | (0b01 << 2) | (0b01 << 4));   // Set PA0, PA1, PA2 en output


    GPIOB->MODER &= ~0b1111;        // PB0 en mode input (00) et PB1

    GPIOB->PUPDR &= ~0b1111;        // Clear PUPDR bits for PB0
    GPIOB->PUPDR |=  0b0101;        // Pull-up activé (01)

    GPIOA->OTYPER &= ~0b111;   // PA0,1,2 en push-pull (0)




    GPIOA->ODR |= (1<<0);              // PA0(rouge) à l'état haut (ex. allumer LED)
	GPIOA->ODR  &= ~(1<<1);        // PA1(jaune) à l'état basse
	GPIOA->ODR  &= ~(1<<2);

    RCC->APB2ENR |= (1 << 14);    // Enable SYSCFG clock

    SYSCFG->EXTICR[0] &= ~0xF;    // Clear EXTI0 config
    SYSCFG->EXTICR[0] |= 0x2;     // Connect PB0 to EXTI0 (EXTI0 ← PB0 )

    // Il te manque ici :
    // - Config EXTI0 (rising/falling edge)
	EXTI->IMR|=0b1;
	EXTI->RTSR &= ~0b1;   // Désactive le front montant sur EXTI0
	EXTI->FTSR |= 0b1;
	pwm_init();

	NVIC_EnableIRQ(EXTI0_IRQn);
	typedef enum {
		IDLE,
		Parking_full,
		car_present_in,
		valid_car,
		invalid_car
	} state;
	volatile state current_state = IDLE;



    while(1){

    	uint8_t car_detected_in=read_PB0_state();
    	switch (current_state){
    	case IDLE:
    		if (car_detected_in){

    		    GPIOA->ODR &=~(1<<0);              // PA0(rouge) à l'état basse
    		    GPIOA->ODR |= (1<<1);     // PA1(jaune) à l'état haute
    			GPIOA->ODR  &= ~(1<<2);
    			spi_transmit('P');
    			current_state= car_present_in ;
    			}
    		else {
    		    GPIOA->ODR |= (1<<0);              // PA0(rouge) à l'état haut (ex. allumer LED)
    			GPIOA->ODR  &= ~(1<<1);        // PA1(jaune) à l'état basse
    			GPIOA->ODR  &= ~(1<<2);
    		}
    		break;

    	case car_present_in:
    	    char sms_get = 0;
    	    do {
    	        sms_get = spi_receive();  // blocking or polling
    	    } while (sms_get != 'V' && sms_get != 'R'); // Wait until valid response

    	    if (sms_get == 'V') {
    	        current_state = valid_car;
    	    } else {
    	        current_state = invalid_car;
    	    }
    	    break;
    	case valid_car :
    		open_in();
    		delay_ms(20000);
    		close_in();
    		nbre_car= nbre_car - 1;
    		if (nbre_car > 0){
    			current_state=IDLE;
    		}
    		else {
    			current_state = Parking_full ;
    		}
    		break;
    	case invalid_car :
    	    GPIOA->ODR |= (1<<0);              // PA0(rouge) à l'état haut (ex. allumer LED)
    		GPIOA->ODR  &= ~(1<<1);        // PA1(jaune) à l'état basse
    		GPIOA->ODR  &= ~(1<<2);
    		close_in();
    		current_state=IDLE;
    		break;




    	case Parking_full :
    		close_in();
    	    GPIOA->ODR |= (1<<0);              // PA0(rouge) à l'état haut (ex. allumer LED)
    		GPIOA->ODR  &= ~(1<<1);        // PA1(jaune) à l'état basse
    		GPIOA->ODR  &= ~(1<<2);
    		break;
    	}
    }
}
