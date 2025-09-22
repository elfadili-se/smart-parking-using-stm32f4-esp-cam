#include "function.h"
#include "stm32f4xx.h"

void SPI_INIT(void){

    //-----------------------------------
    // 1) Activer l'horloge GPIOA et SPI1
    //-----------------------------------
    RCC->AHB1ENR |= (1<<0);    // GPIOA clock enable
    RCC->APB2ENR |= (1<<12);   // SPI1 clock enable

    //-----------------------------------
    // 2) Configurer PA4, PA5, PA6, PA7 en Alternate Function (AF5 pour SPI1)
    //-----------------------------------
    // Effacer les bits MODER pour PA4..PA7
    GPIOA->MODER &= ~(0b11<<(4*2) | 0b11<<(5*2) | 0b11<<(6*2) | 0b11<<(7*2));
    // Mettre Alternate Function (10)
    GPIOA->MODER |=  (0b10<<(4*2) | 0b10<<(5*2) | 0b10<<(6*2) | 0b10<<(7*2));

    // Configurer l'Alternate Function : AF5 = 0101
    // AFRL : 4 bits par pin => PA4=bits[19:16], PA5=[23:20], PA6=[27:24], PA7=[31:28]
    GPIOA->AFR[0] &= ~(0xFFFF << 16);        // Clear AF for pins 4..7
    GPIOA->AFR[0] |=  (0x5555 << 16);        // AF5 (0101) pour PA4..PA7

    // Mettre Very High Speed pour ces pins
    GPIOA->OSPEEDR |= (0b11<<(4*2) | 0b11<<(5*2) | 0b11<<(6*2) | 0b11<<(7*2));

    // Push-pull
    GPIOA->OTYPER &= ~(0b1111<<4);

    // Pas de pull-up / pull-down
    GPIOA->PUPDR &= ~(0b11<<(4*2) | 0b11<<(5*2) | 0b11<<(6*2) | 0b11<<(7*2));

    //-----------------------------------
    // 3) Configurer SPI1->CR1
    //-----------------------------------
    /*
       Bits importants :
       - MSTR = 1 => mode maître
       - BR[2:0] = prescaler (ici /16)
       - CPOL=0, CPHA=0 => mode SPI (mode 0)
       - SSM=1 et SSI=1 => NSS géré en logiciel
       - DFF=0 => 8 bits
       - SPE=1 => activer SPI
    */
    SPI1->CR1 = 0;              // Reset CR1

    SPI1->CR1 |= (1<<2);        // MSTR = 1 (maître)
    SPI1->CR1 |= (0b011<<3);    // BR = fPCLK/16
    SPI1->CR1 &= ~(1<<11);      // DFF=0 (8 bits)
    SPI1->CR1 &= ~(1<<1);       // CPHA=0
    SPI1->CR1 &= ~(1<<0);       // CPOL=0
    SPI1->CR1 |= (1<<9);        // SSM = 1 (NSS logiciel)
    SPI1->CR1 |= (1<<8);        // SSI = 1 (force NSS high)
    SPI1->CR1 |= (1<<6);        // SPE = 1 (activer SPI)

    //-----------------------------------
    // 4) CR2 inutile en mode bloquant simple
    //-----------------------------------
    // SPI1->CR2 = 0;
}



//   ENVOYER UN OCTET EN SPI1    // Attendre que la réception soit terminée (RXNE=1)

//=============================
void spi_transmit(char data){

    // Attendre que le buffer TX soit vide (TXE=1)
    while (!(SPI1->SR & (1<<1)));

    // Ecrire la donnée dans le Data Register
    SPI1->DR = data;

    // Attendre que la réception soit terminée (RXNE=1)    // Attendre que la réception soit terminée (RXNE=1)

    while (!(SPI1->SR & (1<<0)));

    // Lire DR pour vider le buffer (valeur reçue en même temps que l'envoi)
    volatile uint8_t dummy = SPI1->DR;
    (void)dummy; // éviter un warning

    // Attendre que le SPI ne soit plus occupé (BSY=0)
    while (SPI1->SR & (1<<7));
}

uint8_t spi_receive(void) {
    // Attendre que des données soient reçues (RXNE = 1)
    while (!(SPI1->SR & (1 << 0)));

    // Lire les données reçues
    return SPI1->DR;
}
void pwm_init(void) {
    // 1. Enable clocks
    RCC->APB2ENR |= (1 << 0);   // TIM1 clock enable
    RCC->AHB1ENR |= (1 << 0);   // GPIOA clock enable

    // 2. Configure PA8 (CH1) and PA9 (CH2) as alternate function
    GPIOA->MODER &= ~((0b11 << (8 * 2)) | (0b11 << (9 * 2)));  // Clear mode bits
    GPIOA->MODER |=  ((0b10 << (8 * 2)) | (0b10 << (9 * 2)));  // Set to alternate function

    // Select AF1 (TIM1) for PA8 and PA9
    GPIOA->AFR[1] &= ~((0xF << 0) | (0xF << 4));  // Clear AFR[1] bits for PA8/PA9
    GPIOA->AFR[1] |=  ((0x1 << 0) | (0x1 << 4));  // AF1 = TIM1_CH1 and TIM1_CH2

    // Optional: speed, type, no pull-up/down
    GPIOA->OSPEEDR |= ((0b11 << (8 * 2)) | (0b11 << (9 * 2)));  // High speed
    GPIOA->OTYPER  &= ~((1 << 8) | (1 << 9));                   // Push-pull
    GPIOA->PUPDR   &= ~((0b11 << (8 * 2)) | (0b11 << (9 * 2)));  // No pull

    // 3. Configure TIM1
    TIM1->PSC = 83;         // 84 MHz / (83+1) = 1 MHz
    TIM1->ARR = 19999;      // 20 ms period (50Hz)

    // CH1: PA8
    TIM1->CCR1 = 1500;      // 1.5ms = neutral
    TIM1->CCMR1 &= ~(0xFF);                     // Clear CH1 settings
    TIM1->CCMR1 |= (6 << 4) | (1 << 3);         // PWM1 + preload CH1
    TIM1->CCER  |= (1 << 0);                    // Enable CH1 output

    // CH2: PA9
    TIM1->CCR2 = 1500;      // 1.5ms = neutral
    TIM1->CCMR1 &= ~(0xFF00);                   // Clear CH2 settings
    TIM1->CCMR1 |= (6 << 12) | (1 << 11);       // PWM1 + preload CH2
    TIM1->CCER  |= (1 << 4);                    // Enable CH2 output

    // 4. Final timer settings
    TIM1->BDTR |= (1 << 15);   // Main Output Enable (MOE)
    TIM1->CR1  |= (1 << 0);    // Counter Enable
}


void open_in(void){
	TIM1->CCR1 =2000;
}
void close_in (void){
	TIM1->CCR1 =1000;

}
void open_out(void){
	TIM1->CCR2 =2000;
}
void close_out (void){
	TIM1->CCR2 =1000;

}
void delay_ms(uint32_t ms) {

    for(uint32_t i = 0; i < ms * 8000; i++) {
        __asm("nop");
    }
}


uint8_t read_PB1_state(void) {
    return (GPIOB->IDR & (1 << 1)) ? 0 : 1; // inversion
}

uint8_t read_PB0_state(void) {
	    // Read the state of PB0 from IDR
	return (GPIOB->IDR & (1 << 0)) ? 1 : 0;  // for PB0
	}









