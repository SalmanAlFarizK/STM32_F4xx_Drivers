/*
 * stm32f407xx_i2c_driver.c
 *
 *  Created on: Jan 21, 2025
 *      Author: sfkfa
 */

#include "stm32f407xx_i2c_driver.h"

#define WRITE 0
#define READ  1

uint16_t AHB_PreScalar[8] = {2,4,8,16,64,128,256,512};
uint16_t APB1_PreScalar[8] = {2,4,8,16};


static void I2C_GenerateStartCondition(I2C_RegDef_t *pI2Cx);
static void I2C_GenerateStopCondition(I2C_RegDef_t *pI2Cx);
static uint8_t I2C_GetFlagStatus(I2C_RegDef_t *pI2Cx, uint32_t flagName);
static void I2C_ExecteAddressPhase(I2C_RegDef_t *pI2Cx,uint8_t SlaveAddr,uint8_t writeOrRead);
static void I2C_ClearADDRFlag(I2C_Handle_t *pI2CHandle);
static void I2C_MasterHandleRXNEInterrupt(I2C_Handle_t *pI2CHandle);
static void I2C_MasterHandleTXEInterrupt(I2C_Handle_t *pI2CHandle);
static void I2C_CloseReceiveData(I2C_Handle_t *pI2CHandle);
static void I2C_CloseSendData(I2C_Handle_t *pI2CHandle);
/*
 * Not implemented: this function is to get the clock value of PLL clock
 */
uint32_t Rcc_GetPllOutputClk()
{
	return  0;
}

void I2C_PeriClockControl(I2C_RegDef_t *pI2Cx, uint8_t EnorDi)
{
    if (EnorDi == ENABLE)
    {
        if (pI2Cx == I2C1)
        {
        	I2C1_PCLK_EN();
        }
        else if (pI2Cx == I2C2)
        {
        	I2C2_PCLK_EN();
        }
        else if (pI2Cx == I2C3)
        {
        	I2C3_PCLK_EN();
        }
    }
    else if (EnorDi == DISABLE)
    {
        if (pI2Cx == I2C1)
        {
        	I2C1_PCLK_DI();
        }
        else if (pI2Cx == I2C2)
        {
        	I2C2_PCLK_DI();
        }
        else if (pI2Cx == I2C3)
        {
        	I2C3_PCLK_DI();
        }
    }
}

uint32_t Rcc_GetPCLK1Val(void)
{
	uint32_t pclk1,SystemClk;
	uint8_t clkSrc,temp,ahbP,apb1P;

	clkSrc = ((RCC->CFGR >> 2) & 0x3);

	if(clkSrc == 0)
	{
		SystemClk = 16000000;
	}
	else if(clkSrc == 1)
	{
		SystemClk = 8000000;
	}else if(clkSrc == 2)
	{
		SystemClk = Rcc_GetPllOutputClk();
	}

	//for ahb
	temp = ((RCC->CFGR >> 4) & 0xF);

	if(temp < 8)
	{
		ahbP = 1;
	}else{
		ahbP = AHB_PreScalar[temp - 8];
	}

	//for apb1
	temp = ((RCC->CFGR >> 10) & 0x7);

	if(temp < 4)
	{
		apb1P = 1;
	}else{
		apb1P = APB1_PreScalar[temp - 4];
	}

	pclk1 = (SystemClk / ahbP) / apb1P;

	return pclk1;
}

void I2C_Init(I2C_Handle_t *pI2CHandle)
{
	//enable the clock for i2c peripheral
	I2C_PeriClockControl(pI2CHandle->pI2Cx, ENABLE);

	uint32_t tempReg = 0;

	//ACK Control bit
	tempReg |= pI2CHandle->I2C_Config.I2C_ACKControl << I2C_CR1_ACK;
	pI2CHandle->pI2Cx->I2C_CR1 = tempReg;

	//Configure the FREQ field of CR2
	tempReg = Rcc_GetPCLK1Val()/1000000U;
	pI2CHandle->pI2Cx->I2C_CR2 = (tempReg & 0x3F);

	tempReg = pI2CHandle->I2C_Config.I2C_DeviceAddress;
	pI2CHandle->pI2Cx->I2C_OAR1 = tempReg;

	//CCR calculations
	uint16_t ccr_val = 0;
	tempReg = 0;
	if(pI2CHandle->I2C_Config.I2C_SCLSpeed <= I2C_SPEED_SM)
	{
		// mode is standard mode
		ccr_val = Rcc_GetPCLK1Val()/(2 * pI2CHandle->I2C_Config.I2C_SCLSpeed);
		tempReg |= (ccr_val & 0xFFF);
	}else{
		//mode is fast mode
		tempReg |= (1 << 15);
		tempReg |= (pI2CHandle->I2C_Config.I2C_FMDutyCycle << 14);

		if(pI2CHandle->I2C_Config.I2C_FMDutyCycle == I2C_FM_DUTY_2)
		{
			ccr_val = Rcc_GetPCLK1Val()/(3 * pI2CHandle->I2C_Config.I2C_SCLSpeed);
		}else
		{
			ccr_val = Rcc_GetPCLK1Val()/(25 * pI2CHandle->I2C_Config.I2C_SCLSpeed);
		}
		tempReg |= (ccr_val & 0xFFF);
	}
	pI2CHandle->pI2Cx->I2C_CCR = tempReg;

	//TRISE Configuration
	tempReg = 0;
	if(pI2CHandle->I2C_Config.I2C_SCLSpeed <= I2C_SPEED_SM)
	{
		//Mode is Standard
		tempReg = (Rcc_GetPCLK1Val()/1000000U) + 1;
	}else{
		//mode is fast mode
		tempReg = ((Rcc_GetPCLK1Val() * 300) / 1000000000U) + 1;
	}
	pI2CHandle->pI2Cx->I2C_TRISE = (tempReg & 0x3F);
}

void I2C_PeripheralControl(I2C_RegDef_t *pI2Cx,uint8_t EnOrDi)
{
	if(EnOrDi == ENABLE)
	{
		pI2Cx->I2C_CR1 |= (1 << I2C_CR1_PE);
	}else
	{
		pI2Cx->I2C_CR1 &= ~(1 << I2C_CR1_PE);
	}
}

void I2C_GenerateStartCondition(I2C_RegDef_t *pI2Cx)
{
	pI2Cx->I2C_CR1 |= (1 << I2C_CR1_START);
}

void I2C_MasterSendData(I2C_Handle_t *pI2CHandle,uint8_t * pTxBuff,uint32_t len,uint8_t slaveAddr,uint8_t Sr)
{
	/*
	 * Step 1 : Generate the START condition
	 */
	I2C_GenerateStartCondition(pI2CHandle->pI2Cx);

	/*
	 * Step 2: Confirm the start generation is completed by checking the SB flag in SR1
	 * NOTE : Until SB is cleared SCL will be stretched (pulled to LOW)
	 */
	while(! I2C_GetFlagStatus(pI2CHandle->pI2Cx, I2C_SB_FLAG));


	/*
	 * Step 3: Send the address of the slave with r/w bit set to w(0) (total 8 bits)
	 */
	I2C_ExecteAddressPhase(pI2CHandle->pI2Cx,slaveAddr,WRITE);


	/*
	 * Step 4: Confirm the address phase is completed by checking the ADDR flag in the SR1
	 */
	while(! I2C_GetFlagStatus(pI2CHandle->pI2Cx, I2C_ADDR_FLAG));


	/*
	 * Step 5: Clear the ADDR flag according to its software sequence
	 * NOTE : Until ADDR is cleared SCL will be stretched (pulled to low)
	 */
	I2C_ClearADDRFlag(pI2CHandle);

	/*
	 * Step 6: Send the data until the len become zero
	 */
	while(len > 0)
	{
		while(! I2C_GetFlagStatus(pI2CHandle->pI2Cx, I2C_TXE_FLAG)); // Wait until TXE is SET

		pI2CHandle->pI2Cx->I2C_DR = *pTxBuff;
		pTxBuff++;
		len--;
	}

	/*
	 * Step 7: When len becomes zero wait for TXE = 1 and BTF = 1 before generating the STOP condition
	 * NOTE: TXE = 1 BTF = 1 means that both SR and DR are empty and  next transmission should begin
	 * When BTF = 1 SCl will be stretched (pulled to low)
	 */
	while(! I2C_GetFlagStatus(pI2CHandle->pI2Cx, I2C_TXE_FLAG));
	while(! I2C_GetFlagStatus(pI2CHandle->pI2Cx, I2C_BTF_FLAG));


	/*
	 * Step 8: Generate STOP condition and master need not to wait for the completion of STOP condition.
	 * Generating STOP automatically clears the BTF
	 */
	if(I2C_DISABLE_SR == Sr)
	{
		I2C_GenerateStopCondition(pI2CHandle->pI2Cx);
	}
}





void I2C_MasterReceiveData(I2C_Handle_t *pI2CHandle,uint8_t * pRxBuff,uint32_t len,uint8_t slaveAddr,uint8_t Sr)
{
	/*
	 * Step 1 : Generate the START condition
	 */
	I2C_GenerateStartCondition(pI2CHandle->pI2Cx);

	/*
	 * Step 2: Confirm the start generation is completed by checking the SB flag in SR1
	 * NOTE : Until SB is cleared SCL will be stretched (pulled to LOW)
	 */
	while(! I2C_GetFlagStatus(pI2CHandle->pI2Cx, I2C_SB_FLAG));


	/*
	 * Step 3: Send the address of the slave with r/w bit set to R(1) (total 8 bits)
	 */
	I2C_ExecteAddressPhase(pI2CHandle->pI2Cx,slaveAddr,READ);


	/*
	 * Step 4: Confirm the address phase is completed by checking the ADDR flag in the SR1
	 */
	while(! I2C_GetFlagStatus(pI2CHandle->pI2Cx, I2C_ADDR_FLAG));

	if(len == 1)
	{
		//Disable ACKing
		I2C_ManageACKing(pI2CHandle->pI2Cx,I2C_ACK_DISABLE);

		//Clear the ADDR flag
		I2C_ClearADDRFlag(pI2CHandle);

		//wait until RXNE becomes 1
		while(! I2C_GetFlagStatus(pI2CHandle->pI2Cx, I2C_RXNE_FLAG)); // Wait until RXNE is SET

		if(I2C_DISABLE_SR == Sr)
		{
			// generate stop condition
			I2C_GenerateStopCondition(pI2CHandle->pI2Cx);
		}

		//read data into buffer
		*(pRxBuff) = pI2CHandle->pI2Cx->I2C_DR;
	}

	if(len > 1)
	{
		//clear the address flag
		I2C_ClearADDRFlag(pI2CHandle);

		//read the data until the length becomes zero
		for(uint32_t i = len ; i > 0;i--)
		{
			//wait until RXNE becomes 1
			while(! I2C_GetFlagStatus(pI2CHandle->pI2Cx, I2C_RXNE_FLAG)); // Wait until RXNE is SET

			if(i == 2)
			{
				//clear the ACK bit
				I2C_ManageACKing(pI2CHandle->pI2Cx,I2C_ACK_DISABLE);

				if(I2C_DISABLE_SR == Sr)
				{
					// generate stop condition
					I2C_GenerateStopCondition(pI2CHandle->pI2Cx);
				}
			}
			//read the data from data register into the buffer
			*(pRxBuff) = pI2CHandle->pI2Cx->I2C_DR;
			//increment the buffer address
			pRxBuff++;

		}
	}
	// Re enable ACK ing
	I2C_ManageACKing(pI2CHandle->pI2Cx,I2C_ACK_ENABLE);
}


uint8_t I2C_GetFlagStatus(I2C_RegDef_t *pI2Cx, uint32_t flagName)
{
	if(pI2Cx->I2C_SR1 & flagName)
	{
		return FLAG_SET;
	}
	return FLAG_RESET;
}


void I2C_ExecteAddressPhase(I2C_RegDef_t *pI2Cx,uint8_t SlaveAddr,uint8_t writeOrRead)
{
	SlaveAddr = SlaveAddr << 1; // Address is 7 bits long
	if(WRITE == writeOrRead)
	{
		SlaveAddr &= ~(1); // The lsb is R/nW bit which must be zero for WRITE // Slave address is Slave address + r/nw bit = 0
	}
	else if(READ == writeOrRead)
	{
		SlaveAddr |= 1; // The lsb is R/nW bit which must be one for READ // Slave address is Slave address + r/nw bit = 0
	}

	pI2Cx->I2C_DR = SlaveAddr;
}

void I2C_ClearADDRFlag(I2C_Handle_t *pI2CHandle)
{
	uint32_t dummyRead;
	//check the device mode
	if(pI2CHandle->pI2Cx->I2C_SR2 & ( 1 << I2C_SR2_MSL))
	{
		//Device in master mode
		if(I2C_BUSY_IN_RX == pI2CHandle->TxRxState)
		{
			if(1 == pI2CHandle->RxSize)
			{
				//1. Disable the ACK
				I2C_ManageACKing(pI2CHandle->pI2Cx,I2C_ACK_DISABLE);

				//2. Clear ADDR flag (read SR1 and SR2)
				dummyRead = pI2CHandle->pI2Cx->I2C_SR1;
				dummyRead = pI2CHandle->pI2Cx->I2C_SR2;

				(void)dummyRead;

			}
			else
			{
				//Clear ADDR flag (read SR1 and SR2)
				dummyRead = pI2CHandle->pI2Cx->I2C_SR1;
				dummyRead = pI2CHandle->pI2Cx->I2C_SR2;

				(void)dummyRead;
			}
		}
	}else{
		//device in slave mode
		//Clear ADDR flag (read SR1 and SR2)
		dummyRead = pI2CHandle->pI2Cx->I2C_SR1;
		dummyRead = pI2CHandle->pI2Cx->I2C_SR2;

		(void)dummyRead;
	}
}
void I2C_GenerateStopCondition(I2C_RegDef_t *pI2Cx)
{
	pI2Cx->I2C_CR1 |= (1 << I2C_CR1_STOP);
}

void I2C_ManageACKing(I2C_RegDef_t *pI2Cx,uint8_t EnOrDi)
{
	if(I2C_ACK_DISABLE == EnOrDi)
	{
		//Disable the ACk
		pI2Cx->I2C_CR1 &= ~(1 << I2C_CR1_ACK);
	}else if(I2C_ACK_ENABLE == EnOrDi)
	{
		//Enable the ACK
		pI2Cx->I2C_CR1 |= (1 << I2C_CR1_ACK);
	}
}


uint8_t I2C_MasterSendDataIT(I2C_Handle_t *pI2CHandle,uint8_t * pTxBuff,uint32_t len,uint8_t slaveAddr,uint8_t Sr)
{

	uint8_t busystate = pI2CHandle->TxRxState;

	if( (busystate != I2C_BUSY_IN_TX) && (busystate != I2C_BUSY_IN_RX))
	{
		pI2CHandle->pTxBuff = pTxBuff;
		pI2CHandle->txLen = len;
		pI2CHandle->TxRxState = I2C_BUSY_IN_TX;
		pI2CHandle->DevAddr = slaveAddr;
		pI2CHandle->Sr = Sr;

		//Implement code to Generate START Condition
		I2C_GenerateStartCondition(pI2CHandle->pI2Cx);


		//Implement the code to enable ITBUFEN Control Bit
		pI2CHandle->pI2Cx->I2C_CR2 |= ( 1 << I2C_CR2_ITBUFEN);

		//Implement the code to enable ITEVFEN Control Bit
		pI2CHandle->pI2Cx->I2C_CR2 |= ( 1 << I2C_CR2_ITEVTEN);

		//Implement the code to enable ITERREN Control Bit
		pI2CHandle->pI2Cx->I2C_CR2 |= ( 1 << I2C_CR2_ITERREN);

	}

	return busystate;
}



uint8_t I2C_MasterReceiveDataIT(I2C_Handle_t *pI2CHandle,uint8_t * pRxBuff,uint32_t len,uint8_t slaveAddr,uint8_t Sr)
{

	uint8_t busystate = pI2CHandle->TxRxState;

	if( (busystate != I2C_BUSY_IN_TX) && (busystate != I2C_BUSY_IN_RX))
	{
		pI2CHandle->pTxBuff = pRxBuff;
		pI2CHandle->rxLen = len;
		pI2CHandle->TxRxState = I2C_BUSY_IN_RX;
		pI2CHandle->DevAddr = slaveAddr;
		pI2CHandle->Sr = Sr;

		//Implement code to Generate START Condition
		I2C_GenerateStartCondition(pI2CHandle->pI2Cx);


		//Implement the code to enable ITBUFEN Control Bit
		pI2CHandle->pI2Cx->I2C_CR2 |= ( 1 << I2C_CR2_ITBUFEN);

		//Implement the code to enable ITEVFEN Control Bit
		pI2CHandle->pI2Cx->I2C_CR2 |= ( 1 << I2C_CR2_ITEVTEN);

		//Implement the code to enable ITERREN Control Bit
		pI2CHandle->pI2Cx->I2C_CR2 |= ( 1 << I2C_CR2_ITERREN);

	}

	return busystate;
}


void I2C_IRQConfig(uint8_t IRQNumber, uint8_t EnorDi)
{
	if(EnorDi == ENABLE)
	{
		if(IRQNumber <= 31)
		{
			// Pogram ISER0 register (Interrupt set enable 0 register // refer
			*NVIC_ISER0 |= (1 << IRQNumber);
		}
		else if(IRQNumber > 31 && IRQNumber < 64)
		{
			//Program ISER1 register
			*NVIC_ISER1 |= (1 << (IRQNumber % 32));
		}
		else if(IRQNumber >= 64 && IRQNumber <96)
		{
			/*Program ISER 2 register*/
			*NVIC_ISER2 |= (1 << (IRQNumber % 64));

		}
	}
	else
	{
		if(IRQNumber <= 31)
		{
			// Pogram ICER0 register (Interrupt clear enable 0 register // refer
			*NVIC_ICER0 |= (1 << IRQNumber);
		}
		else if(IRQNumber > 31 && IRQNumber < 64)
		{
			//Program ICER1 register
			*NVIC_ICER1 |= (1 << (IRQNumber % 32));
		}
		else if(IRQNumber >= 64 && IRQNumber <96)
		{
			/*Program ICER 2 register*/
			*NVIC_ICER2 |= (1 << (IRQNumber % 64));

		}
	}

}


void I2C_IRQPriorityConfig(uint8_t IRQNumber,uint32_t IRQPriority)
{
	//First lets find out the IPR register
	uint8_t iprx = IRQNumber / 4;
	uint8_t iprx_section = IRQNumber % 4;
	uint8_t shiftAmount = (8 * iprx_section) + (8 - NO_PR_BITS_IMPLEMENTED);

	*(NVIC_PR_BASE_ADDR + iprx ) |= (IRQPriority << shiftAmount);
}

/*
 *  Interrupt handling for interrupts generated by i2c events
 */
void I2C_EV_IRQHandling(I2C_Handle_t *pI2CHandle)
{
	uint32_t temp1,temp2,temp3;


	temp1 = pI2CHandle->pI2Cx->I2C_CR2 & (1 << I2C_CR2_ITEVTEN);
	temp2 = pI2CHandle->pI2Cx->I2C_CR2 & (1 << I2C_CR2_ITBUFEN);


	//1. Handle for interrupt generated by SB event
	//NOTE: SB flag is only applicable in Master mode
	temp3 = pI2CHandle->pI2Cx->I2C_SR1 & (1 << I2C_SR1_SB);
	if(temp1 && temp3)
	{
		//The interrupt is generated because of SB event
		//This block will not be executed in the the slave mode because for slave SB is always zero
		//In this block lets execute the address phase
		if(I2C_BUSY_IN_TX == pI2CHandle->TxRxState)
		{
			I2C_ExecteAddressPhase(pI2CHandle->pI2Cx, pI2CHandle->DevAddr, WRITE);
		}
		else if(I2C_BUSY_IN_RX == pI2CHandle->TxRxState)
		{
			I2C_ExecteAddressPhase(pI2CHandle->pI2Cx, pI2CHandle->DevAddr, READ);
		}
	}

	//2. Handle for interrupt generated by ADDR event
	//NOTE: When Master Mode : Address is sent
	//      When Slave  Mode : Address matched with own address
	temp3 = pI2CHandle->pI2Cx->I2C_SR1 & (1 << I2C_SR1_ADDR);
	if(temp1 && temp3)
	{
		//ADDR is set
		I2C_ClearADDRFlag(pI2CHandle);
	}


	//3. Handle for interrupt generated by BTF(Byte transfer finished) event
	temp3 = pI2CHandle->pI2Cx->I2C_SR1 & (1 << I2C_SR1_BTF);
	if(temp1 && temp3)
	{
		//BTF flag is set
		//Make sure that the TXE is also set
		if(I2C_BUSY_IN_TX == pI2CHandle->TxRxState)
		{
			if(pI2CHandle->txLen == 0)
			{
				//BTF, TXE = 1
				//1. Generate STOP condition
				if(I2C_DISABLE_SR == pI2CHandle->Sr)
				{
					I2C_GenerateStopCondition(pI2CHandle->pI2Cx);
				}

				//2. Reset all the member elements of the handle structure
				I2C_CloseSendData(pI2CHandle);

				//3.Notify the application about the transmission complete

			}
		}
		else if(I2C_BUSY_IN_RX == pI2CHandle->TxRxState)
		{
			//Nothing should be done in this part
			;
		}
	}


	//4. Handle for interrupt generated by STOPF event
	//NOTE: Stop detection flag is applicable only slave mode
	temp3 = pI2CHandle->pI2Cx->I2C_SR1 & (1 << I2C_SR1_STOPF);
	if(temp1 && temp3)
	{
		//STOP flag is set
		//Clear the STOPF. For that first read SR1 followed by writing to CR1 (read SR1 is already done in temp3= operation
		pI2CHandle->pI2Cx->I2C_CR1 |= 0x0000;

		//Notify the application that the STOP is detected
	}


	//5. Handle for interrupt generated by TXE event
	temp3 = pI2CHandle->pI2Cx->I2C_SR1 & (1 << I2C_SR1_TxE);
	if(temp1 && temp2 && temp3)
	{
		//Check for device mode
		//Do all these operations only if the device is in MASTER mode
		if(pI2CHandle->pI2Cx->I2C_SR2 & (1 << I2C_SR2_MSL))
		{
			//TXE flag is set
			I2C_MasterHandleTXEInterrupt(pI2CHandle);
		}
	}


	//6. Handle for interrupt generated by RXNE event
	temp3 = pI2CHandle->pI2Cx->I2C_SR1 & (1 << I2C_SR1_RxNE);
	if(temp1 && temp2 && temp3)
	{
		//RXNE flag is set
		//Do all these operations only if the device is in MASTER mode
		if(pI2CHandle->pI2Cx->I2C_SR2 & (1 << I2C_SR2_MSL))
		{
			I2C_MasterHandleRXNEInterrupt(pI2CHandle);
		}
	}

}

/*
 *  Interrupt handling for interrupts generated by i2c errors
 */

void I2C_ER_IRQHandling(I2C_Handle_t *pI2CHandle)
{

	uint32_t temp1,temp2;

    //Know the status of  ITERREN control bit in the CR2
	temp2 = (pI2CHandle->pI2Cx->I2C_CR2) & ( 1 << I2C_CR2_ITERREN);


    /***********************Check for Bus error************************************/
	temp1 = (pI2CHandle->pI2Cx->I2C_SR1) & ( 1<< I2C_SR1_BERR);
	if(temp1  && temp2 )
	{
		//This is Bus error

		//Implement the code to clear the buss error flag
		pI2CHandle->pI2Cx->I2C_SR1 &= ~( 1 << I2C_SR1_BERR);

		//Implement the code to notify the application about the error
	   //I2C_ApplicationEventCallback(pI2CHandle,I2C_ERROR_BERR);
	}

    /***********************Check for arbitration lost error************************************/
	temp1 = (pI2CHandle->pI2Cx->I2C_SR1) & ( 1 << I2C_SR1_ARLO );
	if(temp1  && temp2)
	{
		//This is arbitration lost error

		//Implement the code to clear the arbitration lost error flag
		pI2CHandle->pI2Cx->I2C_SR1 &= ~( 1 << I2C_SR1_ARLO);

		//Implement the code to notify the application about the error

	}

    /***********************Check for ACK failure  error************************************/

	temp1 = (pI2CHandle->pI2Cx->I2C_SR1) & ( 1 << I2C_SR1_AF);
	if(temp1  && temp2)
	{
		//This is ACK failure error

	    //Implement the code to clear the ACK failure error flag
		pI2CHandle->pI2Cx->I2C_SR1 &= ~( 1 << I2C_SR1_AF);

		//Implement the code to notify the application about the error
	}

    /***********************Check for Overrun/underrun error************************************/
	temp1 = (pI2CHandle->pI2Cx->I2C_SR1) & ( 1 << I2C_SR1_OVR);
	if(temp1  && temp2)
	{
		//This is Overrun/underrun

	    //Implement the code to clear the Overrun/underrun error flag
		pI2CHandle->pI2Cx->I2C_SR1 &= ~( 1 << I2C_SR1_OVR);

		//Implement the code to notify the application about the error
	}

   /***********************Check for Time out error************************************/
	temp1 = (pI2CHandle->pI2Cx->I2C_SR1) & ( 1 << I2C_SR1_TIMEOUT);
	if(temp1  && temp2)
	{
		//This is Time out error

	    //Implement the code to clear the Time out error flag
		pI2CHandle->pI2Cx->I2C_SR1 &= ~( 1 << I2C_SR1_TIMEOUT);

		//Implement the code to notify the application about the error
	}

}

static void I2C_MasterHandleRXNEInterrupt(I2C_Handle_t *pI2CHandle)
{
	//We have to do the data reception
	if(I2C_BUSY_IN_RX == pI2CHandle->TxRxState)
	{
		if(pI2CHandle->RxSize == 1)
		{
			*(pI2CHandle->pRxBuff) = pI2CHandle->pI2Cx->I2C_DR;
			pI2CHandle->rxLen--;
		}

		if(pI2CHandle->RxSize > 1)
		{
			if(pI2CHandle->RxSize == 2)
			{
				//clear the ACK bit
				I2C_ManageACKing(pI2CHandle->pI2Cx, DISABLE);
			}
			//read data register
			*pI2CHandle->pRxBuff = pI2CHandle->pI2Cx->I2C_DR;
			pI2CHandle->pRxBuff++;
			pI2CHandle->rxLen--;
		}
		if(pI2CHandle->rxLen == 0)
		{
			//Close the I2C data reception and notify the application

			//1. Generate the STOP condition
			if(I2C_DISABLE_SR == pI2CHandle->TxRxState)
			{
				I2C_GenerateStopCondition(pI2CHandle->pI2Cx);
			}

			//2. Close the I2C rX
			I2C_CloseReceiveData(pI2CHandle);

			//3. Notify the application
		}
	}

}


static void I2C_MasterHandleTXEInterrupt(I2C_Handle_t *pI2CHandle)
{

	//We have to do the data transmission
	if(I2C_BUSY_IN_TX == pI2CHandle->TxRxState)
	{
		if(pI2CHandle->txLen > 0)
		{
			//1. Load the data into the DR
			pI2CHandle->pI2Cx->I2C_DR = *(pI2CHandle->pTxBuff);

			//2.Decrement the txLen
			pI2CHandle->txLen--;

			//3.Increment the Buffer address
			pI2CHandle->pTxBuff++;
		}
	}
}



static void I2C_CloseReceiveData(I2C_Handle_t *pI2CHandle)
{
	//Implement the code to disable the ITBUFEN control bit
	pI2CHandle->pI2Cx->I2C_CR2 &= ~(1 << I2C_CR2_ITBUFEN);

	//Implement the code to disable ITEVFEN control bit
	pI2CHandle->pI2Cx->I2C_CR2 &= ~(1 << I2C_CR2_ITEVTEN);

	pI2CHandle->TxRxState = I2C_READY;
	pI2CHandle->pRxBuff = NULL;
	pI2CHandle->RxSize = 0;
	pI2CHandle->rxLen = 0;
	if(pI2CHandle->I2C_Config.I2C_ACKControl == I2C_ACK_ENABLE)
	{
		I2C_ManageACKing(pI2CHandle->pI2Cx, ENABLE);
	}
}


static void I2C_CloseSendData(I2C_Handle_t *pI2CHandle)
{
	//Implement the code to disable the ITBUFEN control bit
	pI2CHandle->pI2Cx->I2C_CR2 &= ~(1 << I2C_CR2_ITBUFEN);

	//Implement the code to disable ITEVFEN control bit
	pI2CHandle->pI2Cx->I2C_CR2 &= ~(1 << I2C_CR2_ITEVTEN);

	pI2CHandle->TxRxState = I2C_READY;
	pI2CHandle->pTxBuff = NULL;
	pI2CHandle->txLen = 0;
}
