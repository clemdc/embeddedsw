/*******************************************************************************
 *
 * Copyright (C) 2014 Xilinx, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Use of the Software is limited solely to applications:
 * (a) running on a Xilinx device, or
 * (b) that interact with a Xilinx device through a bus or interconnect. 
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * XILINX CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the Xilinx shall not be used
 * in advertising or otherwise to promote the sale, use or other dealings in
 * this Software without prior written authorization from Xilinx.
 *
*******************************************************************************/
/******************************************************************************/
/**
 *
 * @file xdptx_timer_example.c
 *
 * Contains a design example using the XDptx driver with a user-defined hook
 * for delay. The reasoning behind this is that MicroBlaze sleep is not very
 * accurate without a hardware timer. For systems that have a hardware timer,
 * the user may override the default MicroBlaze sleep with a function that will
 * use the hardware timer.
 *
 * @note        This example requires an AXI timer in the system.
 * @note        For this example to display output, the user will need to
 *              implement initialization of the system (Dptx_PlatformInit) and,
 *              after training is complete, implement configuration of the video
 *              stream source in order to provide the DisplayPort core with
 *              input (Dptx_ConfigureStreamSrc - called in
 *              xdptx_example_common.c). See XAPP1178 for reference.
 * @note        The functions Dptx_PlatformInit and Dptx_ConfigureStreamSrc are
 *              declared extern in xdptx_example_common.h and are left up to the
 *              user to implement.
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date     Changes
 * ----- ---- -------- -----------------------------------------------
 * 1.00a als  06/17/14 Initial creation.
 * </pre>
 *
*******************************************************************************/

/******************************* Include Files ********************************/

#include "xdptx.h"
#include "xdptx_example_common.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xstatus.h"
#include "xtmrctr.h"

/**************************** Function Prototypes *****************************/

u32 Dptx_TimerExample(XDptx *InstancePtr, u16 DeviceId,
                XTmrCtr *TimerCounterPtr, XDptx_TimerHandler UserSleepFunc);
static void Dptx_CustomWaitUs(void *InstancePtr, u32 MicroSeconds);

/*************************** Variable Declarations ****************************/

XTmrCtr TimerCounterInst;       /* The timer counter instance. */

/**************************** Function Definitions ****************************/

/******************************************************************************/
/**
 * This function is the main function of the XDptx timer example.
 *
 * @param       None.
 *
 * @return      - XST_SUCCESS if the timer example finished successfully.
 *              - XST_FAILURE otherwise.
 *
 * @note        None.
 *
*******************************************************************************/
int main(void)
{
        int Status;

        /* Run the XDptx timer example. */
        Status = Dptx_TimerExample(&DptxInstance, DPTX_DEVICE_ID,
                                &TimerCounterInst, &Dptx_CustomWaitUs);
        if (Status != XST_SUCCESS) {
                return XST_FAILURE;
        }

        return XST_SUCCESS;
}

/******************************************************************************/
/**
 * The main entry point for the timer example using the XDptx driver. This
 * function will set up the system and the custom sleep handler. If this is
 * successful, link training will commence and a video stream will start being
 * sent over the main link.
 *
 * @param       InstancePtr is a pointer to the XDptx instance.
 * @param       DeviceId is the unique device ID of the DisplayPort TX core
 *              instance.
 * @param       TimerCounterPtr is a pointer to the timer instance.
 * @param       UserSleepFunc is a pointer to the custom handler for sleep.
 *
 * @return      - XST_SUCCESS if the system was set up correctly and link
 *                training was successful.
 *              - XST_FAILURE otherwise.
 *
 * @note        None.
 *
*******************************************************************************/
u32 Dptx_TimerExample(XDptx *InstancePtr, u16 DeviceId,
                XTmrCtr *TimerCounterPtr, XDptx_TimerHandler UserSleepFunc)
{
        u32 Status;

        /* Do platform initialization here. This is hardware system specific -
         * it is up to the user to implement this function. */
        Dptx_PlatformInit();
        /*******************/

        /* Set a custom timer handler for improved delay accuracy on MicroBlaze
         * systems since the driver does not assume/have a dependency on the
         * system having a timer in the FPGA.
         * Note: This only has an affect for MicroBlaze systems since the Zynq
         * ARM SoC contains a timer, which is used when the driver calls the
         * delay function. */
        XDptx_SetUserTimerHandler(InstancePtr, UserSleepFunc, TimerCounterPtr);

        Status = Dptx_SetupExample(InstancePtr, DeviceId);
        if (Status != XST_SUCCESS) {
                return XST_FAILURE;
        }

        XDptx_EnableTrainAdaptive(InstancePtr, TRAIN_ADAPTIVE);
        XDptx_SetHasRedriverInPath(InstancePtr, TRAIN_HAS_REDRIVER);

        /* A sink monitor must be connected at this point. See the polling or
         * interrupt examples for how to wait for a connection event. */
        Status = Dptx_Run(InstancePtr);
        if (Status != XST_SUCCESS) {
                return XST_FAILURE;
        }

        return XST_SUCCESS;
}

/******************************************************************************/
/**
 * This function is used to override the driver's default sleep functionality.
 * For MicroBlaze systems, the XDptx_WaitUs driver function's default behavior
 * is to use the MB_Sleep function from microblaze_sleep.h, which is implemented
 * in software and only has millisecond accuracy. For this reason, using a
 * hardware timer is preferrable. For ARM/Zynq SoC systems, the SoC's timer is
 * used - XDptx_WaitUs will ignore this custom timer handler.
 *
 * @param       InstancePtr is a pointer to the XDptx instance.
 *
 * @return      None.
 *
 * @note        Use the XDptx_SetUserTimerHandler driver function to set this
 *              function as the handler for when the XDptx_WaitUs driver
 *              function is called.
 *
*******************************************************************************/
static void Dptx_CustomWaitUs(void *InstancePtr, u32 MicroSeconds)
{
        XDptx *XDptx_InstancePtr = (XDptx *)InstancePtr;
        u32 TimerVal;

        XTmrCtr_Start(XDptx_InstancePtr->UserTimerPtr, 0);

        /* Wait specified number of useconds. */
        do {
                TimerVal = XTmrCtr_GetValue(XDptx_InstancePtr->UserTimerPtr, 0);
        }
        while (TimerVal < (MicroSeconds *
                        (XDptx_InstancePtr->Config.SAxiClkHz / 1000000)));

        XTmrCtr_Stop(XDptx_InstancePtr->UserTimerPtr, 0);
}
