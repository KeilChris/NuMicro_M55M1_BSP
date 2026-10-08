/**************************************************************************//**
 * @file     ImageSensor.h
 * @version  V1.00
 * @brief    image sensor capture function
 *
 * @copyright (C) 2023 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#ifndef __IMAGE_SENSOR_H__
#define __IMAGE_SENSOR_H__

#include <stdbool.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    eIMAGE_FMT_YUV422 = 0,
    eIMAGE_FMT_RGB555,
    eIMAGE_FMT_RGB565,
    eIMAGE_FMT_ONLY_Y,
    eIMAGE_FMT_ONLY_Y_1BIT,
    eIMAGE_FMT_RGB888_U8,
    eIMAGE_FMT_BGR888_U8,
    eIMAGE_FMT_RGB888_I8,
    eIMAGE_FMT_BGR888_I8,
    eIMAGE_FMT_ARGB888_U8,
    eIMAGE_FMT_BGRA888_U8,
    eIMAGE_FMT_ARGB888_I8,
    eIMAGE_FMT_BGRA888_I8,
} E_IMAGE_FMT;

#define IMAGE_SENSOR_EVENT_FRAME_COMPLETE (1UL << 0)
#define IMAGE_SENSOR_EVENT_ERROR          (1UL << 1)

typedef void (*ImageSensor_EventCallback_t)(uint32_t events);

/* Single foreground owner; callback runs in the CCAP IRQ and must not call
 * capture/configuration APIs. Buffer ownership ends only after successful
 * completion/abort. A failed abort quarantines the buffer until system reset.
 * This is a vendor BSP contract, not a standard CMSIS camera driver API. */
#define IMAGE_SENSOR_CAPTURE_CONTRACT_VERSION 2
#define IMAGE_SENSOR_OK            0
#define IMAGE_SENSOR_ERROR        (-1)
#define IMAGE_SENSOR_BUSY         (-2)
#define IMAGE_SENSOR_INVALID      (-3)
#define IMAGE_SENSOR_NOT_READY    (-4)
#define IMAGE_SENSOR_TIMEOUT      (-5)
#define IMAGE_SENSOR_FAULT        (-6)

/* Iteration bound, NOT milliseconds. Applications needing a wall-clock limit
 * should poll ImageSensor_PollCaptureDone with their own timer. */
#ifndef IMAGE_SENSOR_CAPTURE_POLL_LIMIT
#define IMAGE_SENSOR_CAPTURE_POLL_LIMIT 1000000U
#endif

/* Returns BUSY until the one-shot engine stops. OK releases a complete frame;
 * ERROR releases a stopped but invalid frame. Other failures do not release
 * an active buffer. Buffer must be nonzero, 32-byte aligned, DMA-accessible and
 * large enough; caller owns cache maintenance and padded buffer allocation. */
int ImageSensor_PollCaptureDone(void);
/* Graceful frame-boundary stop; at most poll_limit + 1 stop checks. Zero
 * performs a single check. No force-disable/reset or unproven DMA drain.
 * OK releases/discards the frame; TIMEOUT/FAULT forbids reuse and new capture. */
int ImageSensor_AbortCapture(uint32_t poll_limit);

int ImageSensor_Init(void);
int ImageSensor_Capture(uint32_t u32FrameBufAddr);
int ImageSensor_Config(E_IMAGE_FMT eImgFmt, uint32_t u32ImgWidth, uint32_t u32ImgHeight, bool bKeepRatio);
int ImageSensor_TriggerCapture(uint32_t u32FrameBufAddr);
int ImageSensor_WaitCaptureDone(void);
void ImageSensor_SetEventCallback(ImageSensor_EventCallback_t callback);
uint32_t ImageSensor_GetCaptureEvents(void);

#ifdef __cplusplus
}
#endif

#endif
