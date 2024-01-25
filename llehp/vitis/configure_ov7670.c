#include "xparameters.h"
#include "xiicps.h"
#include "xil_printf.h"
#include "sleep.h"

#include "camera_config.h"

#define IIC_DEVICE_ID	XPAR_XIICPS_0_DEVICE_ID
#define IIC_SLAVE_ADDR	0x21
#define IIC_SCLK_RATE	400000

int configure_ov7670(void)
{
	int Status, i;
	XIicPs_Config *Config;
	XIicPs Iic;
	struct 

	Config = XIicPs_LookupConfig(IIC_DEVICE_ID);
	if (NULL == Config) {
		xil_printf("No IIC Config found for ID %d\r\n", IIC_DEVICE_ID);
		return XST_FAILURE;
	}

	Status = XIicPs_CfgInitialize(&Iic, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		xil_printf("IIC Cfg Initialize failed\r\n");
		return XST_FAILURE;
	}

	Status = XIicPs_SelfTest(&Iic);
	if (Status != XST_SUCCESS) {
		xil_printf("IIC Self Test failed\r\n");
		return XST_FAILURE;
	}

	XIicPs_SetSClk(&Iic, IIC_SCLK_RATE);

	Status = XIicPs_MasterSendPolled(&Iic, &reset, sizeof(reset), IIC_SLAVE_ADDR);
	while (XIicPs_BusIsBusy(&Iic)) {
		/* NOP */
	}
	if (Status != XST_SUCCESS) {
		xil_printf("IIC Reset OV7670 failed\r\n");
		return XST_FAILURE;
	}
	usleep(10); /* OV7670 datasheet says 1 ms */


	for (i = 0; i < sizeof(ov7670_default_regs) / sizeof(struct regval_list) - 1; i++) {
		Status = XIicPs_MasterSendPolled(&Iic,
				&ov7670_default_regs[i],
				sizeof(struct regval_list),
				IIC_SLAVE_ADDR);
		while (XIicPs_BusIsBusy(&Iic)) {
			/* NOP */
		}
		if (Status != XST_SUCCESS) {
			xil_printf("IIC Write OV7670 Default Regs failed\r\n");
			return XST_FAILURE;
		}
	}

	for (i = 0; i < sizeof(ov7670_fmt_rgb565) / sizeof(struct regval_list) - 1; i++) {
		Status = XIicPs_MasterSendPolled(&Iic,
				&ov7670_fmt_rgb565[i],
				sizeof(struct regval_list),
				IIC_SLAVE_ADDR);
		while (XIicPs_BusIsBusy(&Iic)) {
			/* NOP */
		}
		if (Status != XST_SUCCESS) {
			xil_printf("IIC Write OV7670 Fmt RGB565 failed\r\n");
			return XST_FAILURE;
		}
	}

	return XST_SUCCESS;
}
