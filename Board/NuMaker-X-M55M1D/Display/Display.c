/**************************************************************************//**
 * @file     Display.c
 * @version  V1.10
 * @brief    NuMaker-TFT-LCD5 V1.2 board display support
 *
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @copyright Copyright (C) 2026 Arm Limited and/or its affiliates.
 ******************************************************************************/

#include "NuMicro.h"
#include "Display.h"
#include "LCD.h"

#if !defined(__EBI_LCD_PANEL__) || !defined(LT7381_LCD_PANEL)
#error "NuMaker-TFT-LCD5 V1.2 requires the EBI LT7381 configuration"
#endif

static S_LCD_INFO *const s_lcd = &g_s_WVGA_LT7381;

static void Display_ConfigurePins(void)
{
    SET_EBI_AD0_PA5();
    SET_EBI_AD1_PA4();
    SET_EBI_AD2_PC2();
    SET_EBI_AD3_PC3();
    SET_EBI_AD4_PC4();
    SET_EBI_AD5_PC5();
    SET_EBI_AD6_PD8();
    SET_EBI_AD7_PD9();
    SET_EBI_AD8_PE14();
    SET_EBI_AD9_PE15();
    SET_EBI_AD10_PE1();
    SET_EBI_AD11_PE0();
    SET_EBI_AD12_PH8();
    SET_EBI_AD13_PH9();
    SET_EBI_AD14_PH10();
    SET_EBI_AD15_PH11();
    SET_EBI_nWR_PJ9();
    SET_EBI_nRD_PJ8();
    SET_EBI_nCS0_PD14();
    SET_EBI_ADR0_PH7();

    GPIO_SetSlewCtl(PH, BIT7 | BIT8 | BIT9 | BIT10 | BIT11, GPIO_SLEWCTL_HIGH);
    GPIO_SetSlewCtl(PA, BIT4 | BIT5, GPIO_SLEWCTL_HIGH);
    GPIO_SetSlewCtl(PC, BIT2 | BIT3 | BIT4 | BIT5, GPIO_SLEWCTL_HIGH);
    GPIO_SetSlewCtl(PD, BIT8 | BIT9 | BIT14, GPIO_SLEWCTL_HIGH);
    GPIO_SetSlewCtl(PE, BIT0 | BIT1 | BIT14 | BIT15, GPIO_SLEWCTL_HIGH);
    GPIO_SetSlewCtl(PJ, BIT8 | BIT9, GPIO_SLEWCTL_HIGH);
    GPIO_SetMode(CONFIG_LCD_BACKLIGHT_PORT, 1UL << CONFIG_LCD_BACKLIGHT_PIN,
                 GPIO_MODE_OUTPUT);
    GPIO_SetMode(CONFIG_LCD_RESET_PORT, 1UL << CONFIG_LCD_RESET_PIN,
                 GPIO_MODE_OUTPUT);
}

int Display_Init(void)
{
    uint32_t registers_were_locked = SYS_IsRegLocked();

    if (registers_were_locked != 0U)
    {
        SYS_UnlockReg();
    }
    CLK_EnableModuleClock(CONFIG_LCD_EBI_CLK_MODULE);
#if (CONFIG_DISP_USE_PDMA != 0)
    CLK_EnableModuleClock(PDMA0_MODULE);
    CLK_EnableModuleClock(PDMA1_MODULE);
#endif
    Display_ConfigurePins();
    EBI_Open(CONFIG_LCD_EBI, EBI_BUSWIDTH_16BIT, EBI_TIMING_FAST,
             EBI_OPMODE_CACCESS | EBI_OPMODE_ADSEPARATE, EBI_CS_ACTIVE_LOW);
    EBI_SetBusTiming(CONFIG_LCD_EBI,
                     EBI_TCTL_RAHDOFF_Msk | EBI_TCTL_WAHDOFF_Msk |
                     (4UL << EBI_TCTL_TACC_Pos),
                     EBI_MCLKDIV_2);
    if (registers_were_locked != 0U)
    {
        SYS_LockReg();
    }
    return s_lcd->m_pfnInit();
}

void Display_FillRect(uint16_t *pixels, const S_DISP_RECT *rect, int scale)
{
    int32_t width = (int32_t)rect->u32BottonRightX -
                    (int32_t)rect->u32TopLeftX + 1;
    int32_t height = (int32_t)rect->u32BottonRightY -
                     (int32_t)rect->u32TopLeftY + 1;

    s_lcd->m_pfnSetColumn((uint16_t)rect->u32TopLeftX,
                          (uint16_t)rect->u32BottonRightX);
    s_lcd->m_pfnSetPage((uint16_t)rect->u32TopLeftY,
                        (uint16_t)rect->u32BottonRightY);
    s_lcd->m_pfnSentPixel(pixels, (uint32_t)width, (uint32_t)height, 0U,
                          width * height * (int32_t)sizeof(uint16_t) * scale,
                          scale);
}

void Display_Delay(uint32_t milliseconds)
{
    CLK_SysTickDelay(milliseconds * 1000U);
}

int Display_PutText(const char *text, uint32_t size, uint32_t x, uint32_t y,
                    uint32_t foreground, uint32_t background,
                    bool multiple_lines, int scale)
{
    (void)multiple_lines;
    for (uint32_t i = 0U; i < size; ++i)
    {
        if (x + 8U * (uint32_t)scale > s_lcd->m_u16Width)
        {
            x = 0U;
            y += 16U * (uint32_t)scale;
        }
        if (y + 16U * (uint32_t)scale > s_lcd->m_u16Height)
        {
            return 1;
        }
        s_lcd->m_pfnPutChar((uint16_t)x, (uint16_t)y, (uint8_t)text[i],
                            foreground, background, scale);
        x += 8U * (uint32_t)scale;
    }
    return 0;
}

void Display_ClearRect(uint32_t colour, const S_DISP_RECT *rect)
{
    int32_t width = (int32_t)rect->u32BottonRightX -
                    (int32_t)rect->u32TopLeftX + 1;
    int32_t height = (int32_t)rect->u32BottonRightY -
                     (int32_t)rect->u32TopLeftY + 1;

    s_lcd->m_pfnSetColumn((uint16_t)rect->u32TopLeftX,
                          (uint16_t)rect->u32BottonRightX);
    s_lcd->m_pfnSetPage((uint16_t)rect->u32TopLeftY,
                        (uint16_t)rect->u32BottonRightY);
    s_lcd->m_pfnSentPixel(NULL, (uint32_t)width, (uint32_t)height, colour,
                          width * height * (int32_t)sizeof(uint16_t), 1);
}

void Display_ClearLCD(uint32_t colour)
{
    const S_DISP_RECT full = {0U, 0U, s_lcd->m_u16Width - 1U,
                              s_lcd->m_u16Height - 1U};
    Display_ClearRect(colour, &full);
}

uint32_t Disaplay_GetLCDWidth(void)
{
    return s_lcd->m_u16Width;
}

uint32_t Disaplay_GetLCDHeight(void)
{
    return s_lcd->m_u16Height;
}
