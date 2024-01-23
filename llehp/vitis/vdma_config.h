#ifndef VDMA_CONFIG_H
#define VDMA_CONFIG_H

#define PIXEL_BYTES  4
#define FRAME_WIDTH  640
#define FRAME_HEIGHT 480
#define FRAME_BYTES  (FRAME_HEIGHT * FRAME_WIDTH * PIXEL_BYTES)
#define FRAMEBUFFER1 0x1100000
#define FRAMEBUFFER2 0x1200000
#define FRAMEBUFFER3 0x1300000

#define	PARK_PTR_REG 0x28
#define	S2MM_VDMACR 0x30
#define		IRQFrameCount_SHAMT 16
#define		IRQFrameCount	(0xFF << IRQFrameCount_SHAMT)
#define		GenlockSrc		(1 << 7)
#define		GenlockEn		(1 << 3)
#define		Reset			(1 << 2)
#define		Circular_Park	(1 << 1)
#define		RS				(1 << 0)
#define	S2MM_VDMASR 0x34
#define		IRQFrameCntSts_SHAMT 16
#define		IRQFrameCntSts	(0xFF << IRQFrameCntSts_SHAMT)
#define		Halted			(1 << 0)
#define	S2MM_VDMA_IRQ_MASK 0x3C
#define	S2MM_REG_INDEX 0x44
#define	S2MM_VSIZE 0xA0
#define	S2MM_HSIZE 0xA4
#define	S2MM_FRMDLY_STRIDE 0xA8
#define	S2MM_START_ADDRESS1 0xAC
#define	S2MM_START_ADDRESS2 (S2MM_START_ADDRESS1 + 4)
#define	S2MM_START_ADDRESS3 (S2MM_START_ADDRESS2 + 4)

#endif /* VDMA_CONFIG_H */
