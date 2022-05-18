#pragma once
#include <stddef.h>
#include <stdint.h>

#include "hardware/dma.h"

namespace dma {

inline void set_transfer_size(uint dma_channel, dma_channel_transfer_size size) {
    dma_channel_hw_addr(dma_channel)->al1_ctrl =
        (dma_channel_hw_addr(dma_channel)->al1_ctrl & ~DMA_CH0_CTRL_TRIG_DATA_SIZE_BITS) | (((uint)size) << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB);
}

inline uint32_t get_transfer_count(uint dma_channel) {
    return dma_channel_hw_addr(dma_channel)->transfer_count;
}

}  // namespace dma