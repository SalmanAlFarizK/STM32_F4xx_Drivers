/*
 * ds1307.c
 *
 *  Created on: Mar 9, 2025
 *      Author: sfkfa
 */

#include "ds1307.h"

static void ds1307_i2c_pin_config();
static void ds1307_i2c_config();
static void ds1307_write(uint8_t value,uint8_t address);
static uint8_t ds1307_read(uint8_t reg_addr);
static uint8_t binaryToBCD(uint8_t data);
static uint8_t bcdToBinary(uint8_t data);

I2C_Handle_t g_ds1307I2CHandle;


//Return 1 : CH = 1 ; Init Failed
//Return 0 : CH = 0 ; Init Success
uint8_t ds1307_init(void)
{
	//1. Init the I2C pins
	ds1307_i2c_pin_config();

	//2. Init the I2C peripheral
	ds1307_i2c_config();

	//3. Enable The I2C peripheral
	I2C_PeripheralControl(DS1307_I2C, ENABLE);

	//4. Make the clock halt = 0 (CH bit)
	ds1307_write(0x00,DS1307_ADDR_SEC);

	//5. Read the clock halt bit
	uint8_t clockState = ds1307_read(DS1307_ADDR_SEC);

	return ((clockState >> 7)&0x1);
}

void ds1307_setCurrTime(RTC_time_t * rtcTime)
{
	uint8_t seconds,hrs;
	seconds = binaryToBCD(rtcTime->seconds);
	seconds &= ~(1 << 7);
	ds1307_write(seconds, DS1307_ADDR_SEC);

	ds1307_write(binaryToBCD(rtcTime->minutes), DS1307_ADDR_MIN);

	hrs = binaryToBCD(rtcTime->hours);
	if(rtcTime->timeFormat == TIME_FORMAT_24_HRS)
	{
		hrs &= ~(1 << 6);
	}
	else
	{
		hrs |= (1 << 6);
		hrs = (rtcTime->timeFormat == TIME_FORMAT_12_HRS_PM) ? hrs | (1 << 5) : hrs & ~(1 << 5);
	}
	ds1307_write(hrs, DS1307_ADDR_HRS);

}

void ds1307_getCurrTime(RTC_time_t * rtc_time)
{
	uint8_t seconds,hrs;
	seconds = ds1307_read(DS1307_ADDR_SEC);

	seconds &= ~(1 << 7);

	rtc_time->seconds = bcdToBinary(seconds);

	rtc_time->minutes = bcdToBinary(ds1307_read(DS1307_ADDR_MIN));

	hrs = ds1307_read(DS1307_ADDR_HRS);
	if(hrs & (1 << 6))
	{
		//12 Hr Format
		rtc_time->timeFormat = ((hrs & (1 << 6)) == 0) ? TIME_FORMAT_12_HRS_AM : TIME_FORMAT_12_HRS_PM;
	}
	else
	{
		//24 Hr Format
		rtc_time->timeFormat = TIME_FORMAT_24_HRS;
	}
	hrs &= ~(0x3 << 5); //Clear 6th and 5th Bit
	rtc_time->hours = hrs;
}

void ds1307_setCurrDate(RTC_date_t *rtcDate)
{
	ds1307_write(binaryToBCD(rtcDate->date), DS1307_ADDR_DATE);
	ds1307_write(binaryToBCD(rtcDate->day),  DS1307_ADDR_DAY);
	ds1307_write(binaryToBCD(rtcDate->year), DS1307_ADDR_YEAR);
	ds1307_write(binaryToBCD(rtcDate->month),DS1307_ADDR_MONTH);

}

void ds1307_getCurrDate(RTC_date_t * rtc_date)
{
	rtc_date->day   = binaryToBCD(ds1307_read(DS1307_ADDR_DAY));
	rtc_date->month = binaryToBCD(ds1307_read(DS1307_ADDR_MONTH));
	rtc_date->year  = binaryToBCD(ds1307_read(DS1307_ADDR_YEAR));
	rtc_date->date  = binaryToBCD(ds1307_read(DS1307_ADDR_DATE));
}

static void ds1307_i2c_pin_config()
{
	/*
	 * I2C Pin config
	 * PB6 SCL
	 * PB7 SDA
	 */
	GPIO_Handle_t I2C1_Pins;

	I2C1_Pins.pGPIOx = DS1307_I2C_GPIO_PORT;
	I2C1_Pins.GPIO_PinConfig.GPIO_PinMode = GPIO_MODE_ALTFN;
	I2C1_Pins.GPIO_PinConfig.GPIO_PinAltFunMode = 4;
	I2C1_Pins.GPIO_PinConfig.GPIO_PinOpType = GPIO_OP_TYPE_OD;
	I2C1_Pins.GPIO_PinConfig.GPIO_PinPuPdControl = DS1307_I2C_PUPD;
	I2C1_Pins.GPIO_PinConfig.GPIO_PinSpeed = GPIO_SPEED_FAST;

	//SCL
	I2C1_Pins.GPIO_PinConfig.GPIO_PinNumber = DS1307_I2C_SCL_PIN;
	GPIO_Init(&I2C1_Pins);

	//SDA
	I2C1_Pins.GPIO_PinConfig.GPIO_PinNumber = DS1307_I2C_SDA_PIN;
	GPIO_Init(&I2C1_Pins);

}
static void ds1307_i2c_config()
{
	g_ds1307I2CHandle.pI2Cx = DS1307_I2C;
	g_ds1307I2CHandle.I2C_Config.I2C_ACKControl = I2C_ACK_ENABLE;
	g_ds1307I2CHandle.I2C_Config.I2C_SCLSpeed = DS1307_I2C_SPEED;

	I2C_Init(&g_ds1307I2CHandle);

}

static void ds1307_write(uint8_t value,uint8_t reg_address)
{
	uint8_t tx[2] = {reg_address,value};
	I2C_MasterSendData(&g_ds1307I2CHandle, tx, sizeof(tx), DS1307_I2C_ADDR, 0);

}

static uint8_t ds1307_read(uint8_t reg_addr)
{
	uint8_t result;
	I2C_MasterSendData(&g_ds1307I2CHandle, &reg_addr, 1, DS1307_I2C_ADDR, 0);
	I2C_MasterReceiveData(&g_ds1307I2CHandle,&result,1,DS1307_I2C_ADDR, 0);

	return result;
}

static uint8_t binaryToBCD(uint8_t data)
{
	uint8_t bcdVal = (((data /10) <<  4) | (data % 10));
	return bcdVal;
}


static uint8_t bcdToBinary(uint8_t data)
{
	uint8_t binaryVal = (((data >> 4)*10) + (data & 0x0F));
	return binaryVal;
}
