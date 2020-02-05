/****************************************************************************
 * By Joonki Hong, KAIST
 * Z1 specific arch file
 * CPU: MSP430
 *
 */

#include "contiki.h"
#include "contiki-net.h"
#include "dev/leds.h"
#include "dev/spi.h"
#include <stdio.h>
#include "isr_compat.h"
#include "cc1200-arch.h"
#include "cc1200-z1-arch.h"

#if DEBUG_CC1200_ARCH > 0
#define PRINTF(...) printf(__VA_ARGS__)
#define BUSYWAIT_UNTIL(cond, max_time)                                  \
  do {                                                                  \
    rtimer_clock_t t0;                                                  \
    t0 = RTIMER_NOW();                                                  \
    while(!(cond) && RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + (max_time))) {} \
    if(!(RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + (max_time)))) { \
      printf("ARCH: Timeout exceeded in line %d!\n", __LINE__); \
    } \
  } while(0)
#else
#define PRINTF(...)
#define BUSYWAIT_UNTIL(cond, max_time) while(!cond)
#endif


/*------------------------------------------------------------------------
void
cc1200_int_handler(uint8_t port, uint8_t pin)
{
  // To keep the gpio_register_callback happy
  cc1200_rx_interrupt();
}
*/

/*---------------------------------------------------------------------------*/
void
cc1200_arch_spi_select(void)
{
  /* Set CSn to low (0) */
//  GPIO_CLR_PIN(CC1200_SPI_CSN_PORT_BASE, CC1200_SPI_CSN_PIN_MASK);
	CC1200_SPI_CSN_PORT(OUT) &= ~BV(CC1200_SPI_CSN_PIN);

			
	/* The MISO pin should go low before chip is fully enabled. */
//	BUSYWAIT_UNTIL(
//    GPIO_READ_PIN(CC1200_SPI_MISO_PORT_BASE, CC1200_SPI_MISO_PIN_MASK) == 0,
//    RTIMER_SECOND / 100);
	while((CC1200_SPI_MISO_PORT(IN) & BV(CC1200_SPI_MISO_PIN)) != 0);

}
/*---------------------------------------------------------------------------*/
void
cc1200_arch_spi_deselect(void)
{
  /* Set CSn to high (1) */
  // GPIO_SET_PIN(CC1200_SPI_CSN_PORT_BASE, CC1200_SPI_CSN_PIN_MASK);
  CC1200_SPI_CSN_PORT(OUT) |= BV(CC1200_SPI_CSN_PIN);

}
/*------------------------------------------------------------------------
 * MODIFIED
 * ---*/

int
cc1200_arch_spi_rw_byte(uint8_t c)
{
  SPI_WAITFORTx_BEFORE();
//  SPIX_BUF(CC1200_SPI_INSTANCE) = c;
  SPI_TXBUF = c;
//  SPIX_WAITFOREOTx(CC1200_SPI_INSTANCE);
  SPI_WAITFOREOTx();
//  SPIX_WAITFOREORx(CC1200_SPI_INSTANCE);
  SPI_WAITFOREORx();
//  c = SPIX_BUF(CC1200_SPI_INSTANCE);
  c = SPI_RXBUF;

  return c;
}


/*---------------------------------------------------------------------------
 * MODIFIED
 * */
int
cc1200_arch_spi_rw(uint8_t *inbuf, const uint8_t *outbuf, uint16_t len)
{
  int i;

  if((inbuf == NULL && outbuf == NULL) || len <= 0) {
    return 1;
  } else if(inbuf == NULL) {
    for(i = 0; i < len; i++) {
    	SPI_WRITE(outbuf[i]); 
    }
  } else if(outbuf == NULL) {
    for(i = 0; i < len; i++) {
   		SPI_READ(inbuf[i]);
		}
  } else {
    for(i = 0; i < len; i++) {
	SPI_WAITFORTx_BEFORE();
//      SPIX_BUF(CC1200_SPI_INSTANCE) = write_buf[i];
	SPI_TXBUF = outbuf[i];
//      SPIX_WAITFOREOTx(CC1200_SPI_INSTANCE);
	SPI_WAITFOREOTx();
//      SPIX_WAITFOREORx(CC1200_SPI_INSTANCE);
	SPI_WAITFOREORx();
//	inbuf[i] = SPIX_BUF(CC1200_SPI_INSTANCE);
	inbuf[i] = SPI_RXBUF;
    }
  }
  return 0;
}

/*---------------------------------------------------------------------------*/
void
cc1200_arch_init(void)
{
  spi_init();
	
	CC1200_SPI_RESET_PORT(SEL) &= ~ BV(CC1200_SPI_RESET_PIN);
	CC1200_SPI_RESET_PORT(DIR) |= BV(CC1200_SPI_RESET_PIN);
	CC1200_SPI_RESET_PORT(OUT) |= BV(CC1200_SPI_RESET_PIN);

	/* Set CSN direction as output */
	CC1200_SPI_CSN_PORT(DIR) |= BV(CC1200_SPI_CSN_PIN);

	/* Disable radio */
	cc1200_arch_spi_deselect();
	
	/* Set the direction */
	CC1200_GPIO0_PORT(SEL) &= ~BV(CC1200_GPIO0_PIN);
	CC1200_GPIO0_PORT(DIR) &= ~BV(CC1200_GPIO0_PIN);

	CC1200_GPIO2_PORT(SEL) &= ~BV(CC1200_GPIO2_PIN);
	CC1200_GPIO2_PORT(DIR) &= ~BV(CC1200_GPIO2_PIN);

	/* Reset the CC1200 */
	CC1200_SPI_CLK_PORT(OUT) |= BV(CC1200_SPI_CLK_PIN);
	CC1200_SPI_MOSI_PORT(OUT) |= BV(CC1200_SPI_MOSI_PIN);

	CC1200_SPI_CSN_PORT(OUT) &= ~BV(CC1200_SPI_CSN_PIN);
	CC1200_SPI_CSN_PORT(OUT) |= BV(CC1200_SPI_CSN_PIN);

	clock_delay_usec(400);
	
	/* Rising edge interrupt */
	
	// CC1200_GPIO0_PORT(IES) &= ~ BV(CC1200_GPIO0_PIN);
	// CC1200_GPIO2_PORT(IES) &= ~ BV(CC1200_GPIO2_PIN);

	CC1200_SPI_CSN_PORT(OUT) &= ~ BV(CC1200_SPI_CSN_PIN);
	while ((CC1200_SPI_MISO_PORT(IN) & BV(CC1200_SPI_MISO_PIN)) != 0);
}

void
cc1200_arch_interrupt_enable(void)
{
	/* Reset interrupt trigger*/
	CC1200_GPIO0_PORT(IFG) &= ~ BV(CC1200_GPIO0_PIN);
	CC1200_GPIO2_PORT(IFG) &= ~ BV(CC1200_GPIO2_PIN);

	/* Enable interrupt on the GPIO0 pin*/
	CC1200_GPIO0_PORT(IE) |= BV(CC1200_GPIO0_PIN);
	CC1200_GPIO2_PORT(IE) |= BV(CC1200_GPIO2_PIN);
}

void
clock_delay_usec(uint16_t usec)
{
	clock_delay(usec/100);
}

/* NOW this is handled in the ADXL345 accelerometer code as it uses irq on port1 too.

ISR(PORT1, cc1200_port1_interrupt)
{ 
	
	ENERGEST_ON(ENERGEST_TYPE_IRQ);

	// if (CC1200_GPIO0_PORT(IFG) & BV(CC1200_GPIO0_PIN) | CC1200_GPIO2_PORT(IFG) & BV(CC1200_GPIO2_PIN) ){
	if (CC1200_GPIO2_PORT(IFG) & BV(CC1200_GPIO2_PIN)){
			if (cc1200_rx_interrupt()){
				LPM4_EXIT;
			}
	}
	
	// Reset interrupt trigger 
	// CC1200_GPIO0_PORT(IFG) &= ~BV(CC1200_GPIO0_PIN);
	CC1200_GPIO2_PORT(IFG) &= ~BV(CC1200_GPIO2_PIN);

	ENERGEST_OFF(ENERGEST_TYPE_IRQ);
}
*/


void
cc1200_arch_gpio0_setup_irq(int rising)
{
	/*
  GPIO_SOFTWARE_CONTROL(CC1200_GDO0_PORT_BASE, CC1200_GDO0_PIN_MASK);
  GPIO_SET_INPUT(CC1200_GDO0_PORT_BASE, CC1200_GDO0_PIN_MASK);
  GPIO_DETECT_EDGE(CC1200_GDO0_PORT_BASE, CC1200_GDO0_PIN_MASK);
  GPIO_TRIGGER_SINGLE_EDGE(CC1200_GDO0_PORT_BASE, CC1200_GDO0_PIN_MASK);

  if(rising) {
    GPIO_DETECT_RISING(CC1200_GDO0_PORT_BASE, CC1200_GDO0_PIN_MASK);
  } else {
    GPIO_DETECT_FALLING(CC1200_GDO0_PORT_BASE, CC1200_GDO0_PIN_MASK);
  }

  GPIO_ENABLE_INTERRUPT(CC1200_GDO0_PORT_BASE, CC1200_GDO0_PIN_MASK);
  ioc_set_over(CC1200_GDO0_PORT, CC1200_GDO0_PIN, IOC_OVERRIDE_PUE);
  nvic_interrupt_enable(CC1200_GPIOx_VECTOR);
  gpio_register_callback(cc1200_int_handler, CC1200_GDO0_PORT,
                         CC1200_GDO0_PIN);
	*/

	/* Set the direction */
	CC1200_GPIO0_PORT(SEL) &= ~BV(CC1200_GPIO0_PIN);
	CC1200_GPIO0_PORT(DIR) &= ~BV(CC1200_GPIO0_PIN);
	/* CLR,0: rising edge, SET,1: falling edge */
	if(rising){
		CC1200_GPIO0_PORT(IES) &= ~ BV(CC1200_GPIO0_PIN);
		printf("GPIO0 is set as rising edge\n");
	}
	else
	{
		CC1200_GPIO0_PORT(IES) |= BV(CC1200_GPIO0_PIN);
		printf("GPIO0 is set as falling edge\n");
	}


	/* Reset interrupt trigger*/
	// CC1200_GPIO0_PORT(IFG) &= ~ BV(CC1200_GPIO0_PIN);
	/* Enable interrupt on the GPIO0 pin*/
	CC1200_GPIO0_PORT(IE) |= BV(CC1200_GPIO0_PIN);
}




void
cc1200_arch_gpio2_setup_irq(int rising)
{
	/*
  GPIO_SOFTWARE_CONTROL(CC1200_GDO2_PORT_BASE, CC1200_GDO2_PIN_MASK);
  GPIO_SET_INPUT(CC1200_GDO2_PORT_BASE, CC1200_GDO2_PIN_MASK);
  GPIO_DETECT_EDGE(CC1200_GDO2_PORT_BASE, CC1200_GDO2_PIN_MASK);
  GPIO_TRIGGER_SINGLE_EDGE(CC1200_GDO2_PORT_BASE, CC1200_GDO2_PIN_MASK);

  if(rising) {
    GPIO_DETECT_RISING(CC1200_GDO2_PORT_BASE, CC1200_GDO2_PIN_MASK);
  } else {
    GPIO_DETECT_FALLING(CC1200_GDO2_PORT_BASE, CC1200_GDO2_PIN_MASK);
  }

  GPIO_ENABLE_INTERRUPT(CC1200_GDO2_PORT_BASE, CC1200_GDO2_PIN_MASK);
  ioc_set_over(CC1200_GDO2_PORT, CC1200_GDO2_PIN, IOC_OVERRIDE_PUE);
  nvic_interrupt_enable(CC1200_GPIOx_VECTOR);
  gpio_register_callback(cc1200_int_handler, CC1200_GDO2_PORT,
                         CC1200_GDO2_PIN);
	*/
	
	/* Set the direction */
	CC1200_GPIO2_PORT(SEL) &= ~BV(CC1200_GPIO2_PIN);

	CC1200_GPIO2_PORT(DIR) &= ~BV(CC1200_GPIO2_PIN);
	/* CLR,0: rising edge, SET,1: falling edge */
	if (rising)
	{
		CC1200_GPIO2_PORT(IES) &= ~ BV(CC1200_GPIO2_PIN);
		printf("GPIO2 is set as rising edge\n");
	}
	else{
		CC1200_GPIO2_PORT(IES) |= BV(CC1200_GPIO2_PIN);
		printf("GPIO2 is set as falling edge\n");
	}

	/* Reset interrupt trigger*/
	// CC1200_GPIO2_PORT(IFG) &= ~ BV(CC1200_GPIO2_PIN);
	/* Enable interrupt on the GPIO0 pin*/
	CC1200_GPIO2_PORT(IE) |= BV(CC1200_GPIO2_PIN);
}


void
cc1200_arch_gpio0_enable_irq(void)
{
  // GPIO_ENABLE_INTERRUPT(CC1200_GDO0_PORT_BASE, CC1200_GDO0_PIN_MASK);
  // ioc_set_over(CC1200_GDO0_PORT, CC1200_GDO0_PIN, IOC_OVERRIDE_PUE);
  // nvic_interrupt_enable(CC1200_GPIOx_VECTOR);

	// For msp430
	// Reset the interrupt flag
	CC1200_GPIO0_PORT(IFG) &= ~ BV(CC1200_GPIO0_PIN);

	// Enable the interrupt
	CC1200_GPIO0_PORT(IE) |= BV(CC1200_GPIO0_PIN);

}

void
cc1200_arch_gpio0_disable_irq(void)
{
  // GPIO_DISABLE_INTERRUPT(CC1200_GDO0_PORT_BASE, CC1200_GDO0_PIN_MASK);

	// For msp430
	CC1200_GPIO0_PORT(IE) &= ~ BV(CC1200_GPIO0_PIN);
}

void
cc1200_arch_gpio2_enable_irq(void)
{
  // GPIO_ENABLE_INTERRUPT(CC1200_GDO2_PORT_BASE, CC1200_GDO2_PIN_MASK);
  // ioc_set_over(CC1200_GDO2_PORT, CC1200_GDO2_PIN, IOC_OVERRIDE_PUE);
  // nvic_interrupt_enable(CC1200_GPIOx_VECTOR);

	// For msp430
	// Reset the interrupt flag
	CC1200_GPIO2_PORT(IFG) &= ~ BV(CC1200_GPIO2_PIN);

	// Enable the interrupt
	CC1200_GPIO2_PORT(IE) |= BV(CC1200_GPIO2_PIN);
}

void
cc1200_arch_gpio2_disable_irq(void)
{
  // GPIO_DISABLE_INTERRUPT(CC1200_GDO2_PORT_BASE, CC1200_GDO2_PIN_MASK);

	// For msp430
	CC1200_GPIO2_PORT(IE) &= ~ BV(CC1200_GPIO2_PIN);
}

int
cc1200_arch_gpio0_read_pin(void)
{
  // return GPIO_READ_PIN(CC1200_GDO0_PORT_BASE, CC1200_GDO0_PIN_MASK);
	return CC1200_GPIO0_PORT(IN) & BV(CC1200_GPIO0_PIN);
}

int
cc1200_arch_gpio2_read_pin(void)
{
  // return GPIO_READ_PIN(CC1200_GDO2_PORT_BASE, CC1200_GDO2_PIN_MASK);
	return CC1200_GPIO2_PORT(IN) & BV(CC1200_GPIO2_PIN);
}

int
cc1200_arch_gpio3_read_pin(void)
{
  return 0x00;
}

