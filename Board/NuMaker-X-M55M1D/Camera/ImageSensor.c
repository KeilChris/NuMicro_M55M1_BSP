/**************************************************************************//**
 * @file     img_sensor.c
 * @version  V1.00
 * @brief    image sensor driver
 *
 * @copyright (C) 2023 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include "NuMicro.h"

#include "ImageSensor.h"
#include "Sensor.h"

#if defined(__ZEPHYR__)
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#endif

typedef struct s_output_format
{
    uint32_t    m_u32OutputFormat;
    uint32_t    m_u32BytePerPixel;
    const char *m_strFormatName;
} S_OUTPUT_FORMAT;

const S_OUTPUT_FORMAT s_sOutputFormat[] =
{
    { CCAP_PKT_OUTFMT_YUV422,         2, "YUV422",         },
    { CCAP_PKT_OUTFMT_RGB555,         2, "RGB555",         },
    { CCAP_PKT_OUTFMT_RGB565,         2, "RGB565",         },
    { CCAP_PKT_OUTFMT_ONLY_Y,         1, "ONLY_Y",         },
    { CCAP_PKT_OUTFMT_ONLY_Y,         1, "1BIT",           },
    { CCAP_PKT_OUTFMT_RGB888_U8,      3, "RGB888_U8",      },
    { CCAP_PKT_OUTFMT_BGR888_U8,      3, "BGR888_U8",      },
    { CCAP_PKT_OUTFMT_RGB888_I8,      3, "RGB888_I8",      },
    { CCAP_PKT_OUTFMT_BGR888_I8,      3, "BGR888_I8",      },
    { CCAP_PKT_OUTFMT_ARGB888_U8,     4, "ARGB888_U8",     },
    { CCAP_PKT_OUTFMT_BGRA888_U8,     4, "BGRA888_U8",     },
    { CCAP_PKT_OUTFMT_ARGB888_I8,     4, "ARGB888_I8",     },
    { CCAP_PKT_OUTFMT_BGRA888_I8,     4, "BGRA888_I8",     },
};

static volatile uint32_t s_u32CaptureEvents;
static ImageSensor_EventCallback_t s_pfnEventCallback;
static bool s_bInitialized;
static bool s_bConfigured;
static volatile bool s_bCaptureActive;
static bool s_bCaptureFault;

#define CAPTURE_INTERRUPTS (CCAP_INTEN_VIEN_Msk | CCAP_INTEN_MEIEN_Msk)
#define CAPTURE_STATUS (CCAP_INTSTS_VINTF_Msk | CCAP_INTSTS_MEINTF_Msk)

/*------------------------------------------------------------------------------------------*/
/*  CCAP_IRQHandler                                                                          */
/*------------------------------------------------------------------------------------------*/
void CCAP_InterruptHandler(void)
{
    //g_u32FramePass++;
    //printf("Frame count: %d\n", g_u32FramePass);
}

void CCAP_IRQHandler(void)
{
    uint32_t u32CCAP_Status = CCAP->INTSTS;
    uint32_t events = 0U;

    if ((CCAP->INTEN & CCAP_INTEN_VIEN_Msk) && (u32CCAP_Status & CCAP_INTSTS_VINTF_Msk))
    {
        CCAP_InterruptHandler();
        events |= IMAGE_SENSOR_EVENT_FRAME_COMPLETE;
    }

    if ((CCAP->INTEN & CCAP_INTEN_ADDRMIEN_Msk) && (u32CCAP_Status & CCAP_INTSTS_ADDRMINTF_Msk))
    {
        CCAP->INTSTS = CCAP_INTSTS_ADDRMINTF_Msk;
    }

    if ((CCAP->INTEN & CCAP_INTEN_MEIEN_Msk) && (u32CCAP_Status & CCAP_INTSTS_MEINTF_Msk))
    {
        events |= IMAGE_SENSOR_EVENT_ERROR;
    }

    /* W1C only the sampled bits; publish combined completion/error before
     * waking the foreground owner. Never rearm from the callback. */
    CCAP->INTSTS = u32CCAP_Status & CAPTURE_STATUS;
    CCAP->CTL = CCAP->CTL | CCAP_CTL_UPDATE;
    if (s_bCaptureActive)
    {
        /* An already-pending NVIC IRQ can run after the foreground masks
         * INTEN. Preserve its sampled status for PollCaptureDone as well. */
        if ((u32CCAP_Status & CCAP_INTSTS_VINTF_Msk) != 0U)
            s_u32CaptureEvents |= IMAGE_SENSOR_EVENT_FRAME_COMPLETE;
        if ((u32CCAP_Status & CCAP_INTSTS_MEINTF_Msk) != 0U)
            s_u32CaptureEvents |= IMAGE_SENSOR_EVENT_ERROR;
    }
    __DSB();
    __ISB();
    if (s_bCaptureActive && events != 0U && s_pfnEventCallback != NULL)
        s_pfnEventCallback(events);
}


void CCAP_SetFreq(uint32_t u32CCAP_ClkSrc, uint32_t u32SensorFreq)
{
    int32_t  i32Div;
    uint32_t u32CCAP_Clk;

    /* Unlock protected registers */
    SYS_UnlockReg();

    if (u32CCAP_ClkSrc == CLK_CCAPSEL_CCAP0SEL_HIRC)
        u32CCAP_Clk = __HIRC;
    else if (u32CCAP_ClkSrc == CLK_CCAPSEL_CCAP0SEL_MIRC)
        u32CCAP_Clk = CLK_GetMIRCFreq();
    else if (u32CCAP_ClkSrc == CLK_CCAPSEL_CCAP0SEL_HCLK2)
        u32CCAP_Clk = CLK_GetHCLK2Freq();
    else if (u32CCAP_ClkSrc == CLK_CCAPSEL_CCAP0SEL_APLL0_DIV2)
        u32CCAP_Clk = CLK_GetAPLL0ClockFreq() / 2;
    else if (u32CCAP_ClkSrc == CLK_CCAPSEL_CCAP0SEL_HXT)
        u32CCAP_Clk = CLK_GetHXTFreq();
    else
    {
        printf("Invalid CCAP clock source !\n");
        return ;
    }

    CLK_SetModuleClock(CCAP0_MODULE, u32CCAP_ClkSrc, 0);

    if (u32SensorFreq <= u32CCAP_Clk)
    {
        i32Div = (u32CCAP_Clk / u32SensorFreq) - 1;

        if (i32Div < 0)
            i32Div = 0;
    }
    else
    {
        i32Div = 0;
    }

    CLK->VSENSEDIV = (CLK->VSENSEDIV & ~CLK_VSENSEDIV_VSENSEDIV_Msk) | (i32Div << CLK_VSENSEDIV_VSENSEDIV_Pos);

    /* Lock protected registers */
    SYS_LockReg();

    printf("CCAP   engine clock: %d Hz\n", u32CCAP_Clk);
    printf("Target sensor clock: %d Hz.\n", u32SensorFreq);
    printf("Actual sensor clock: %d Hz. Divider=%d+1\n", u32CCAP_Clk / (i32Div + 1), i32Div);
}

static void MFP_ConfigCCAP(uint32_t bConfigCCAP)
{
    if (!bConfigCCAP)
    {
        // Config as GPIO

#if defined(M55M1_ETM_BOARD)
        // ETM Adapater Board Sensor 0
        SET_GPIO_PA1();
        SET_GPIO_PA0();
        SET_GPIO_PC5();
        SET_GPIO_PC4();
        SET_GPIO_PC3();
        SET_GPIO_PC2();
        SET_GPIO_PC1();
        SET_GPIO_PC0();

        SET_GPIO_PG9();
        SET_GPIO_PG10();
        SET_GPIO_PG12();
        SET_GPIO_PD7();
#else
        // NuMaker-M55M1 Sensor 0
        SET_GPIO_PG2();
        SET_GPIO_PG3();
        SET_GPIO_PG4();
        SET_GPIO_PF11();
        SET_GPIO_PF10();
        SET_GPIO_PF9();
        SET_GPIO_PF8();
        SET_GPIO_PF7();

        SET_GPIO_PG9();
        SET_GPIO_PG10();
        SET_GPIO_PG12();
        SET_GPIO_PD7();
#endif

    }
    else
    {

        /* Set multi-function pins for CCAP in DataFlow */

#if defined(M55M1_ETM_BOARD)
        // ETM Adapater Board Sensor 0
        SET_CCAP_DATA7_PA1();
        SET_CCAP_DATA6_PA0();
        SET_CCAP_DATA5_PC5();
        SET_CCAP_DATA4_PC4();
        SET_CCAP_DATA3_PC3();
        SET_CCAP_DATA2_PC2();
        SET_CCAP_DATA1_PC1();
        SET_CCAP_DATA0_PC0();

        SET_CCAP_PIXCLK_PG9();
        SET_CCAP_SCLK_PG10();
        SET_CCAP_VSYNC_PG12();
        SET_CCAP_HSYNC_PD7();

#else
        // NuMaker-M55M1 Sensor 0
        SET_CCAP_DATA7_PG2();
        SET_CCAP_DATA6_PG3();
        SET_CCAP_DATA5_PG4();
        SET_CCAP_DATA4_PF11();
        SET_CCAP_DATA3_PF10();
        SET_CCAP_DATA2_PF9();
        SET_CCAP_DATA1_PF8();
        SET_CCAP_DATA0_PF7();

        SET_CCAP_PIXCLK_PG9();
        SET_CCAP_SCLK_PG10();
        SET_CCAP_VSYNC_PG12();
        SET_CCAP_HSYNC_PD7();
#endif
    }
}

static S_SENSOR_INFO *s_psSensorInfo = NULL;

int ImageSensor_Init(void)
{
    if (s_bCaptureFault) return IMAGE_SENSOR_FAULT;
    if (s_bCaptureActive) return IMAGE_SENSOR_BUSY;
    s_bInitialized = false;
    s_bConfigured = false;
    CCAP->INTEN &= ~CAPTURE_INTERRUPTS;
    /* Init Engine clock and Sensor clock */
    CCAP_SetFreq(CLK_CCAPSEL_CCAP0SEL_HCLK2, 48000000);
    MFP_ConfigCCAP(TRUE);

    /* Init sensor */
    s_psSensorInfo = &g_sSensorHM1055_VGA_YUV422;

    /* Initialize sensor and set sensor output format as YUV422 */
    if (s_psSensorInfo->pfnInitSensor(0) == FALSE) return IMAGE_SENSOR_ERROR;

    s_bInitialized = true;
    return IMAGE_SENSOR_OK;
}

int ImageSensor_Config(E_IMAGE_FMT eImgFmt, uint32_t u32ImgWidth, uint32_t u32ImgHeight, bool bKeepRatio)
{
    uint32_t u32CropWinWidth;
    uint32_t u32CropWinHeight;
    uint32_t u32CropWinX = 0;
    uint32_t u32CropWinY = 0;

    if (s_bCaptureFault) return IMAGE_SENSOR_FAULT;
    if (!s_bInitialized) return IMAGE_SENSOR_NOT_READY;
    if (s_bCaptureActive || !CCAP_IS_STOPPED()) return IMAGE_SENSOR_BUSY;
    if ((unsigned)eImgFmt >= sizeof(s_sOutputFormat) / sizeof(s_sOutputFormat[0]) ||
        eImgFmt == eIMAGE_FMT_ONLY_Y_1BIT ||
        u32ImgWidth == 0U || u32ImgHeight == 0U ||
        u32ImgWidth > s_psSensorInfo->m_u16Width ||
        u32ImgHeight > s_psSensorInfo->m_u16Height ||
        (u32ImgWidth * s_sOutputFormat[eImgFmt].m_u32BytePerPixel) % 4U != 0U)
        return IMAGE_SENSOR_INVALID;

    if (bKeepRatio)
    {
        float fCorpFactoryW;
        float fCorpFactoryH;
        float fCorpFactory;
        fCorpFactoryW = (float)s_psSensorInfo->m_u16Width / u32ImgWidth;
        fCorpFactoryH = (float)s_psSensorInfo->m_u16Height / u32ImgHeight;

        if (fCorpFactoryW > fCorpFactoryH)
            fCorpFactory = fCorpFactoryH;
        else
            fCorpFactory = fCorpFactoryW;

        u32CropWinWidth = (uint32_t)(u32ImgWidth * fCorpFactory);
        u32CropWinHeight = (uint32_t)(u32ImgHeight * fCorpFactory);

        //centre
        u32CropWinX = (s_psSensorInfo->m_u16Width - u32CropWinWidth) / 2;
        u32CropWinY = (s_psSensorInfo->m_u16Height - u32CropWinHeight) / 2;

    }
    else
    {
        u32CropWinWidth = s_psSensorInfo->m_u16Width;
        u32CropWinHeight = s_psSensorInfo->m_u16Height;
    }


    /* Set Cropping Window Vertical/Horizontal Starting Address and Cropping Window Size */
    CCAP_SetCroppingWindow(u32CropWinY, u32CropWinX, u32CropWinHeight, u32CropWinWidth);

    /* Set Vsync polarity, Hsync polarity, pixel clock polarity, Sensor and CCAP format and Order */
    CCAP_OpenPipes(
        s_psSensorInfo->m_u32Polarity,
        s_psSensorInfo->m_u32InputFormat,
        s_sOutputFormat[eImgFmt].m_u32OutputFormat,
        CCAP_PLN_DISABLED
    );

    /* Set Packet Scaling Vertical/Horizontal Factor Register */
    CCAP_SetPacketScaling(u32ImgHeight, u32CropWinHeight, u32ImgWidth, u32CropWinWidth);
    printf("sensor input width %d \n", s_psSensorInfo->m_u16Width);
    printf("sensor input height %d \n", s_psSensorInfo->m_u16Height);
    printf("scaled image width %u \n", u32ImgWidth);
    printf("scaled image height %u \n", u32ImgHeight);

    /* Set Packet Frame Output Pixel Stride Width */
    CCAP_SetPacketStride(u32ImgWidth);

    /* Enable External CAP Interrupt */
    /* On zephyr, use zephyr ISR API */
    CCAP->INTSTS = CCAP_INTSTS_VINTF_Msk | CCAP_INTSTS_MEINTF_Msk;
    CCAP_EnableInt(CCAP_INTEN_VIEN_Msk | CCAP_INTEN_MEIEN_Msk);
#if defined(__ZEPHYR__)
    IRQ_CONNECT(CCAP_IRQn, 0, CCAP_IRQHandler, NULL, 0);
    irq_enable(CCAP_IRQn);
#else
    NVIC_ClearPendingIRQ(CCAP_IRQn);
    NVIC_EnableIRQ(CCAP_IRQn);
#endif

    s_bConfigured = true;
    return IMAGE_SENSOR_OK;
}


int ImageSensor_Capture(uint32_t u32FrameBufAddr)
{
    int result = ImageSensor_TriggerCapture(u32FrameBufAddr);
    if (result != IMAGE_SENSOR_OK) return result;
    return ImageSensor_WaitCaptureDone();
}

int ImageSensor_TriggerCapture(uint32_t u32FrameBufAddr)
{
    if (s_bCaptureFault) return IMAGE_SENSOR_FAULT;
    if (!s_bConfigured) return IMAGE_SENSOR_NOT_READY;
    if (s_bCaptureActive || !CCAP_IS_STOPPED()) return IMAGE_SENSOR_BUSY;
    if (u32FrameBufAddr == 0U || (u32FrameBufAddr & 31U) != 0U)
        return IMAGE_SENSOR_INVALID;
    /* No documented OVF clear operation: reject a sticky overflow rather
     * than silently reusing an uncertain capture configuration. */
    if ((CCAP->FIFOTH & CCAP_FIFOTH_OVF_Msk) != 0U)
    {
        s_bCaptureFault = true;
        return IMAGE_SENSOR_FAULT;
    }
    CCAP->INTEN &= ~CAPTURE_INTERRUPTS;
    s_u32CaptureEvents = 0U;
    CCAP->INTSTS = CAPTURE_STATUS;

    /* Set System Memory Packet Base Address Register */
    //printf("sensor capture address %x \n", u32FrameBufAddr);
    CCAP_SetPacketBuf((uint32_t)u32FrameBufAddr);

    /* Trigger image capture */
    s_bCaptureActive = true;
    CCAP_Start();
    CCAP->CTL |= CCAP_CTL_SHUTTER_Msk;
    CCAP->INTEN |= CAPTURE_INTERRUPTS;

    return 0;
}

int ImageSensor_WaitCaptureDone(void)
{
    uint32_t remaining = IMAGE_SENSOR_CAPTURE_POLL_LIMIT;
    int result;
    while ((result = ImageSensor_PollCaptureDone()) == IMAGE_SENSOR_BUSY)
    {
        if (remaining == 0U)
        {
            (void)ImageSensor_AbortCapture(IMAGE_SENSOR_CAPTURE_POLL_LIMIT);
            return IMAGE_SENSOR_TIMEOUT;
        }
        --remaining;
    }
    return result;
}

int ImageSensor_PollCaptureDone(void)
{
    uint32_t status;
    if (s_bCaptureFault) return IMAGE_SENSOR_FAULT;
    if (!s_bCaptureActive) return IMAGE_SENSOR_NOT_READY;
    if (!CCAP_IS_STOPPED()) return IMAGE_SENSOR_BUSY;
    CCAP->INTEN &= ~CAPTURE_INTERRUPTS;
    __DSB();
    status = CCAP->INTSTS;
    /* Snapshot hardware before retiring IRQ ownership. A pending ISR either
     * already latched/acknowledged these bits, or they remain in status. Once
     * inactive, it cannot race the foreground event read/modify/write below. */
    s_bCaptureActive = false;
    if ((status & CCAP_INTSTS_VINTF_Msk) != 0U)
        s_u32CaptureEvents |= IMAGE_SENSOR_EVENT_FRAME_COMPLETE;
    if ((status & CCAP_INTSTS_MEINTF_Msk) != 0U ||
        (CCAP->FIFOTH & CCAP_FIFOTH_OVF_Msk) != 0U)
        s_u32CaptureEvents |= IMAGE_SENSOR_EVENT_ERROR;
    CCAP->INTSTS = status & CAPTURE_STATUS;
    if ((s_u32CaptureEvents & IMAGE_SENSOR_EVENT_ERROR) != 0U ||
        (s_u32CaptureEvents & IMAGE_SENSOR_EVENT_FRAME_COMPLETE) == 0U)
        return IMAGE_SENSOR_ERROR;
    return IMAGE_SENSOR_OK;
}

int ImageSensor_AbortCapture(uint32_t poll_limit)
{
    if (s_bCaptureFault) return IMAGE_SENSOR_FAULT;
    if (!s_bCaptureActive) return IMAGE_SENSOR_OK;
    CCAP->INTEN &= ~CAPTURE_INTERRUPTS;
    /* TRM Rev 1.02 p.1671: SHUTTER disables capture after a complete frame.
     * Do not force CCAPEN low and mistake the readback for DMA drain. */
    CCAP->CTL |= CCAP_CTL_SHUTTER_Msk;
    while (!CCAP_IS_STOPPED())
    {
        if (poll_limit == 0U)
        {
            s_bCaptureFault = true;
            s_u32CaptureEvents |= IMAGE_SENSOR_EVENT_ERROR;
            return IMAGE_SENSOR_TIMEOUT;
        }
        --poll_limit;
    }
    __DSB();
    CCAP->INTSTS = CAPTURE_STATUS;
    s_bCaptureActive = false;
    return IMAGE_SENSOR_OK;
}

void ImageSensor_SetEventCallback(ImageSensor_EventCallback_t callback)
{
    s_pfnEventCallback = callback;
}

uint32_t ImageSensor_GetCaptureEvents(void)
{
    return s_u32CaptureEvents;
}
