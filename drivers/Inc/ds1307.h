/*
 * ds1307.h
 *
 *  Created on: Mar 9, 2025
 *      Author: sfkfa
 */

#ifndef INC_DS1307_H_
#define INC_DS1307_H_

#include "stm32f407xx.h"
#include "stm32f407xx_i2c_driver.h"
#include "stm32f407xx_gpio_driver.h"

/*
 * Application Configurable Items
 */
#define DS1307_I2C               I2C1
#define DS1307_I2C_GPIO_PORT     GPIOB
#define DS1307_I2C_SDA_PIN       7
#define DS1307_I2C_SCL_PIN       6
#define DS1307_I2C_SPEED         I2C_SPEED_SM
#define DS1307_I2C_PUPD          GPIO_NO_PUPD





/*
 * Register Addresses
 */
#define DS1307_ADDR_SEC   0x00
#define DS1307_ADDR_MIN   0x01
#define DS1307_ADDR_HRS   0x02
#define DS1307_ADDR_DAY   0x03
#define DS1307_ADDR_DATE  0x04
#define DS1307_ADDR_MONTH 0x05
#define DS1307_ADDR_YEAR  0x06


#define TIME_FORMAT_12_HRS_AM  0
#define TIME_FORMAT_12_HRS_PM  1
#define TIME_FORMAT_24_HRS     2


#define DS1307_I2C_ADDR   0x68

#define SUNDAY     1
#define MONDAY     2
#define TUESDAY    3
#define WEDNESDAY  4
#define THURSDAY   5
#define FRIDAY     6
#define SATURDAY   7

typedef struct
{
	uint8_t date;
	uint8_t month;
	uint8_t year;
	uint8_t day;
}RTC_date_t;

typedef struct
{
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
	uint8_t timeFormat;
}RTC_time_t;

/*
 * Function Prototypes
 */

uint8_t ds1307_init(void);

void ds1307_setCurrTime(RTC_time_t *);
void ds1307_getCurrTime(RTC_time_t *);

void ds1307_setCurrDate(RTC_date_t *);
void ds1307_getCurrDate(RTC_date_t *);




#endif /* INC_DS1307_H_ */
