
#include "xparameters.h"
#include "sleep.h"
#include "xiicps.h"
#include "xil_printf.h"
#include "cameraRegisters.h"

#define IIC_DEVICE_ID		XPAR_XIICPS_0_DEVICE_ID

XIicPs Iic;

int iic_write_reg(u8 reg, u8 value)
{
	int Status;
	u8 buff[2];

	buff[0] = reg;
	buff[1] = value;
	//xil_printf("Reg adress:%d, reg value:%d\n", reg, value);
	Status = XIicPs_MasterSendPolled(&Iic, buff, 2, OV7670ADDR);

	if(Status != XST_SUCCESS){
		xil_printf("WriteReg:I2C Write Fail\n");
		return XST_FAILURE;
	}
	// Wait until bus is idle to start another transfer.
	while(XIicPs_BusIsBusy(&Iic)){
		/* NOP */
	}

	usleep(30*1000);	// wait 30ms

	return XST_SUCCESS;
}

int iic_read_reg(u8 reg)
{
	u8 buff[2];

	buff[0] = reg;
	XIicPs_MasterSendPolled(&Iic, buff, 1, OV7670ADDR);
	while(XIicPs_BusIsBusy(&Iic)){
		/* NOP */
	}

	XIicPs_MasterRecvPolled(&Iic, buff, 1, OV7670ADDR);
	while(XIicPs_BusIsBusy(&Iic)){
		/* NOP */
	}

	return buff[0];
}


void wrSensorRegs8_8(const struct regval_list reglist[]){
	int i = 0;
	const struct regval_list *regs = reglist;
	while (regs[i].reg_num != 0xff && regs[i].value != 0xff  ){
		int result = iic_write_reg(regs[i].reg_num, regs[i].value);
		if (result != XST_SUCCESS) {
			xil_printf("Error writing register 0x%02X\n", regs[i].reg_num);
		}
		i++;
		usleep(100);
	}
}

int isCameraConfiguredCorrectly() {
    // Read current configuration from OV7670
    int reg_hstart = iic_read_reg(REG_HSTART);
    int reg_hstop = iic_read_reg(REG_HSTOP);

    int reg_vstart = iic_read_reg(REG_VSTART);
    int reg_vstop = iic_read_reg(REG_VSTOP);

    // Check HSIZE (horizontal size)
    if (reg_hstart != HSTART_VGA) {
        xil_printf("Error: Incorrect HSTART configuration\n");
        return 1;  // Incorrect HSIZE
    }

    if (reg_hstop != HSTOP_VGA) {
        xil_printf("Error: Incorrect HSTOP configuration\n");
        return 1;  // Incorrect HSIZE
    }

    // Check VSIZE (vertical size)
    if (reg_vstart != VSTART_VGA) {
        xil_printf("Error: Incorrect VSTART configuration\n");
        return 1;  // Incorrect VSIZE
    }

    if (reg_vstop != VSTOP_VGA) {
        xil_printf("Error: Incorrect VSTOP configuration\n");
        return 1;  // Incorrect VSIZE
    }

    xil_printf("Camera is correctly configured with VGA resolution and RGB565 color format\n");
    return 0;  // Success
}

int init_ov7670()
{
	int Status, result;
	XIicPs_Config *I2C_Config;	/**< configuration information for the device */

	I2C_Config = XIicPs_LookupConfig(IIC_DEVICE_ID);
	if(I2C_Config == NULL){
		xil_printf("Error: XIicPs_LookupConfig()\n");
		return XST_FAILURE;
	}

	Status = XIicPs_CfgInitialize(&Iic, I2C_Config, I2C_Config->BaseAddress);
	if(Status != XST_SUCCESS){
		xil_printf("Error: XIicPs_CfgInitialize()\n");
		return XST_FAILURE;
	}

	Status = XIicPs_SelfTest(&Iic);
	if(Status != XST_SUCCESS){
		xil_printf("Error: XIicPs_SelfTest()\n");
		return XST_FAILURE;
	}

	XIicPs_SetSClk(&Iic, IIC_SCLK_RATE);
	xil_printf("I2C configuration done.\n");

	xil_printf("Soft Reset OV7670.\n");
	result = iic_write_reg(REG_COM7, COM7_RESET);
	if(result != XST_SUCCESS){
		xil_printf("Error: OV767 RESET\n");
		return XST_FAILURE;
	}
	usleep(300*1000);

	xil_printf("Set ov7670 default regs\n");
	wrSensorRegs8_8(ov7670_default_regs);

	xil_printf("Set resolution\n");
	int reg_com7 = iic_read_reg(REG_COM7);
	//iic_write_reg(REG_COM7,reg_com7|COM7_VGA);
	//wrSensorRegs8_8(ov7670_VGA);

	xil_printf("Set colour\n");
	wrSensorRegs8_8(ov7670_fmt_rgb565);

	xil_printf("Set test\n");
	wrSensorRegs8_8(ov7670_test_bar);

	Status = isCameraConfiguredCorrectly();
	if(Status != XST_SUCCESS){
		xil_printf("Error: Camer not set up correctly\n");
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}
