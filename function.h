#ifndef FUNCTION_H
#define FUNCTION_H

#include <stdint.h>

// Déclarations des fonctions
void SPI_INIT(void);
void spi_transmit(char data);
uint8_t spi_receive();
void pwm_init(void);
void open_in(void);
void close_in(void);
void open_out(void);
void close_out(void);
void delay_ms(uint32_t ms);
uint8_t read_PB1_state(void) ;
uint8_t read_PB0_state(void) ;

#endif
