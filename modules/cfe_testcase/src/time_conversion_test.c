/*************************************************************************
**
**      GSC-18128-1, "Core Flight Executive Version 7.0"
**
**      Copyright (c) 2006-2021 United States Government as represented by
**      the Administrator of the National Aeronautics and Space Administration.
**      All Rights Reserved.
**
**      Licensed under the Apache License, Version 2.0 (the "License");
**      you may not use this file except in compliance with the License.
**      You may obtain a copy of the License at
**
**        http://www.apache.org/licenses/LICENSE-2.0
**
**      Unless required by applicable law or agreed to in writing, software
**      distributed under the License is distributed on an "AS IS" BASIS,
**      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**      See the License for the specific language governing permissions and
**      limitations under the License.
**
** File: time_conversion_test.c
**
** Purpose:
**   Functional test of basic Time Conversion APIs
**
*************************************************************************/

/*
 * Includes
 */
#include "cfe_test.h"

void TestConvertMET2SCTime(void)
{
    UtPrintf("Testing: CFE_TIME_MET2SCTime");

    /* Mission Elapsed Time */
    CFE_TIME_SysTime_t MET;
    /* MET + SCTF */
    CFE_TIME_SysTime_t TAI;
    /* MET + SCTF - Leap Seconds */
    CFE_TIME_SysTime_t UTC;
    /* Spacecraft Time */
    CFE_TIME_SysTime_t SCTime;

    OS_time_t start;
    OS_time_t end;
    OS_time_t difference;

    /* Print buffers */
    char timeBuf1[sizeof("yyyy-ddd-hh:mm:ss.xxxxx_")];
    char timeBuf2[sizeof("yyyy-ddd-hh:mm:ss.xxxxx_")];

    OS_GetLocalTime(&start);

    /* Get Times */
    MET = CFE_TIME_GetMET();
    TAI = CFE_TIME_GetTAI();
    UTC = CFE_TIME_GetUTC();

    OS_GetLocalTime(&end);

    /* Convert - should produce a TAI or UTC at the moment of GetMET() */
    SCTime = CFE_TIME_MET2SCTime(MET);

    /* write Spacecraft Time into second buffer */
    CFE_TIME_Print(timeBuf2, SCTime);

    difference = OS_TimeSubtract(end, start);

    /* Check conversion */
#if (CFE_MISSION_TIME_CFG_DEFAULT_TAI == true)
    /* SCTime is TAI format */

    /* avoid unused compiler warning */
    (void)UTC;

    /* write TAI into first buffer */
    CFE_TIME_Print(timeBuf1, TAI);

    UtAssert_True(TimeInRange(SCTime, TAI, difference), "TAI (%s) = MET2SCTime (%s)", timeBuf1, timeBuf2);
#else
    /* SCTime is UTC format */

    /* avoid unused compiler warning */
    (void)TAI;

    /* write UTC into first buffer */
    CFE_TIME_Print(timeBuf1, UTC);

    UtAssert_True(TimeInRange(SCTime, UTC, difference), "UTC (%s) = MET2SCTime (%s)", timeBuf1, timeBuf2);
#endif
}

void TestConvertSubSeconds2MicroSeconds(void)
{
    UtPrintf("Testing: CFE_TIME_Sub2MicroSecs");

    /* predefined amount of sub-seconds */
    uint32 SUB = 31000;
    /* correct micro-seconds equal to the predefined sub-seconds */
    uint32 ExpectedMS = 7;
    uint32 Sub2Micro;

    /* run Sub2MicroSecs with predefined amount of sub-seconds and save result */
    Sub2Micro = CFE_TIME_Sub2MicroSecs(SUB);

    UtAssert_UINT32_EQ(ExpectedMS, Sub2Micro);
}

void TestConvertMicroSeconds2SubSeconds(void)
{
    UtPrintf("Testing: CFE_TIME_Micro2SubSecs");

    /* predefined micro-seconds */
    uint32 MS = 64512;
    /* predefined sub-seconds equal to predefined ms above */
    uint32 ExpectedSUB = 277076931;
    uint32 Micro2Sub;

    /* convert and assert */
    Micro2Sub = CFE_TIME_Micro2SubSecs(MS);
    UtAssert_UINT32_EQ(ExpectedSUB, Micro2Sub);

    /* assert for ms > 999999 >= 1 second  */
    Micro2Sub = CFE_TIME_Micro2SubSecs(999999 + 1);
    UtAssert_UINT32_EQ(0xFFFFFFFF, Micro2Sub);
}

void TimeConversionTestSetup(void)
{
    UtTest_Add(TestConvertMET2SCTime, NULL, NULL, "Test convert MET into spacecraft time");
    UtTest_Add(TestConvertSubSeconds2MicroSeconds, NULL, NULL, "Test Convert sub-seconds into micro-seconds");
    UtTest_Add(TestConvertMicroSeconds2SubSeconds, NULL, NULL, "Test Convert micro-seconds into sub-seconds");
}
