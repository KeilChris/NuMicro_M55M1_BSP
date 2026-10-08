/**************************************************************************//**
 * @file     Board.c
 * @version  V1.00
 * @brief    NuMaker-X-M55M1D early and run-time board initialization
 *
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @copyright Copyright (C) 2026 Arm Limited and/or its affiliates.
 ******************************************************************************/

#include "NuMicro.h"
#include "hyperram_code.h"
#include "Board.h"

/* This strong startup hook replaces the weak implementation in the DFP.
 * It must not access initialized data or call the C library. */
void Reset_Handler_PreInit(void)
{
    const uint32_t slew = GPIO_SLEWCTL_FAST1;

    SYS_UnlockReg();
    CLK_EnableAPLL(CLK_APLLCTL_APLLSRC_HIRC, FREQ_220MHZ, CLK_APLL0_SELECT);
    CLK_SetSCLK(CLK_SCLKSEL_SCLKSEL_APLL0);
    CLK_SET_HCLK2DIV(2);
    CLK_SET_PCLK0DIV(2);
    CLK_SET_PCLK1DIV(2);
    CLK_SET_PCLK2DIV(2);
    CLK_SET_PCLK3DIV(2);
    CLK_SET_PCLK4DIV(2);

    CLK_EnableModuleClock(GPIOD_MODULE);
    CLK_EnableModuleClock(GPIOF_MODULE);
    CLK_EnableModuleClock(GPIOG_MODULE);
    CLK_EnableModuleClock(GPIOH_MODULE);
    CLK_EnableModuleClock(GPIOJ_MODULE);
    CLK_EnableModuleClock(CCAP0_MODULE);
    CLK_EnableModuleClock(SPIM0_MODULE);

    SET_SPIM0_CLK_PH13();
    SET_SPIM0_D2_PJ5();
    SET_SPIM0_D3_PJ6();
    SET_SPIM0_D4_PH14();
    SET_SPIM0_D5_PH15();
    SET_SPIM0_D6_PG13();
    SET_SPIM0_D7_PG14();
    SET_SPIM0_MISO_PJ4();
    SET_SPIM0_MOSI_PJ3();
    SET_SPIM0_RESETN_PJ2();
    SET_SPIM0_RWDS_PG15();
    SET_SPIM0_SS_PJ7();

    PG->SMTEN |= GPIO_SMTEN_SMTEN13_Msk | GPIO_SMTEN_SMTEN14_Msk |
                 GPIO_SMTEN_SMTEN15_Msk;
    PH->SMTEN |= GPIO_SMTEN_SMTEN13_Msk | GPIO_SMTEN_SMTEN14_Msk |
                 GPIO_SMTEN_SMTEN15_Msk;
    PJ->SMTEN |= GPIO_SMTEN_SMTEN2_Msk | GPIO_SMTEN_SMTEN3_Msk |
                 GPIO_SMTEN_SMTEN4_Msk | GPIO_SMTEN_SMTEN5_Msk |
                 GPIO_SMTEN_SMTEN6_Msk | GPIO_SMTEN_SMTEN7_Msk;
    GPIO_SetSlewCtl(PG, BIT13 | BIT14 | BIT15, slew);
    GPIO_SetSlewCtl(PH, BIT13 | BIT14 | BIT15, slew);
    GPIO_SetSlewCtl(PJ, BIT2 | BIT3 | BIT4 | BIT5 | BIT6 | BIT7, slew);

    HyperRAM_Init(SPIM0);
    SPIM_HYPER_EnterDirectMapMode(SPIM0);
    SYS_LockReg();
}

void Board_Init(void)
{
    InitDebugUart();
}
