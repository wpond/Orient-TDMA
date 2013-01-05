#ifndef __DMA_H__
    
    #define __DMA_H__

    #include "efm32.h"
    #include "efm32_dma.h"

    #define DMA_CHANNEL_RTX 0
    #define DMA_CHANNEL_RRX 1

    /* DMA control block, must be aligned according to number of DMA channels. */
    #if (DMA_CHAN_COUNT > 16) 
        #define DMA_CONTROL_BLOCK_ALIGNMENT 1024 
    #elif (DMA_CHAN_COUNT > 8) 
        #define DMA_CONTROL_BLOCK_ALIGNMENT 512 
    #elif (DMA_CHAN_COUNT > 4) 
        #define DMA_CONTROL_BLOCK_ALIGNMENT 256 
    #elif (DMA_CHAN_COUNT > 2) 
        #define DMA_CONTROL_BLOCK_ALIGNMENT 128 
    #endif

    #if defined (__ICCARM__)
        #pragma data_alignment=DMA_CONTROL_BLOCK_ALIGNMENT
        DMA_DESCRIPTOR_TypeDef dmaControlBlock[DMA_CHAN_COUNT * 2];
    #elif defined (__CC_ARM)
        DMA_DESCRIPTOR_TypeDef dmaControlBlock[DMA_CHAN_COUNT * 2] __attribute__ ((aligned(DMA_CONTROL_BLOCK_ALIGNMENT)));
    #elif defined (__GNUC__)
        DMA_DESCRIPTOR_TypeDef dmaControlBlock[DMA_CHAN_COUNT * 2] __attribute__ ((aligned(DMA_CONTROL_BLOCK_ALIGNMENT)));
    #else
        #error Undefined toolkit, need to define alignment 
    #endif

#endif