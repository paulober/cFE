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
*************************************************************************/

/**
 * @file
 *
 * Functional test of SB transmit/receive APIs
 * CFE_SB_TransmitMsg - Transmit a message.
 * CFE_SB_ReceiveBuffer - Receive a message from a software bus pipe.
 * CFE_SB_AllocateMessageBuffer - Get a buffer pointer to use for "zero copy" SB sends.
 * CFE_SB_ReleaseMessageBuffer - Release an unused "zero copy" buffer pointer.
 * CFE_SB_TransmitBuffer - Transmit a buffer.
 */

#include "cfe_test.h"
#include "cfe_msgids.h"

/* A simple command message */
typedef struct
{
    CFE_MSG_CommandHeader_t CmdHeader;
    uint32                  CmdPayload;
} CFE_FT_TestCmdMessage_t;

/* A simple telemetry message */
typedef struct
{
    CFE_MSG_TelemetryHeader_t TlmHeader;
    uint32                    TlmPayload;
} CFE_FT_TestTlmMessage_t;

/* A message intended to be (overall) larger than the CFE_MISSION_SB_MAX_SB_MSG_SIZE */
typedef struct
{
    CFE_MSG_Message_t Hdr;
    uint8             MaxSize[CFE_MISSION_SB_MAX_SB_MSG_SIZE];
} CFE_FT_TestBigMessage_t;

/*
 * This test procedure should be agnostic to specific MID values, but it should
 * not overlap/interfere with real MIDs used by other apps.
 */
static const CFE_SB_MsgId_t CFE_FT_CMD_MSGID = CFE_SB_MSGID_WRAP_VALUE(CFE_TEST_CMD_MID);
static const CFE_SB_MsgId_t CFE_FT_TLM_MSGID = CFE_SB_MSGID_WRAP_VALUE(CFE_TEST_HK_TLM_MID);

static CFE_FT_TestBigMessage_t CFE_FT_BigMsg;

void TestBasicTransmitRecv(void)
{
    CFE_SB_PipeId_t                PipeId1;
    CFE_SB_PipeId_t                PipeId2;
    CFE_FT_TestCmdMessage_t        CmdMsg;
    CFE_FT_TestTlmMessage_t        TlmMsg;
    CFE_SB_MsgId_t                 MsgId;
    CFE_MSG_SequenceCount_t        Seq1, Seq2;
    CFE_SB_Buffer_t *              MsgBuf;
    const CFE_FT_TestCmdMessage_t *CmdPtr;
    const CFE_FT_TestTlmMessage_t *TlmPtr;

    UtPrintf("Testing: CFE_SB_TransmitMsg");

    /* Setup, create a pipe and subscribe (one cmd, one tlm) */
    UtAssert_INT32_EQ(CFE_SB_CreatePipe(&PipeId1, 5, "TestPipe1"), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_SB_CreatePipe(&PipeId2, 5, "TestPipe2"), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_SB_SubscribeEx(CFE_FT_CMD_MSGID, PipeId1, CFE_SB_DEFAULT_QOS, 3), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_SB_SubscribeEx(CFE_FT_TLM_MSGID, PipeId2, CFE_SB_DEFAULT_QOS, 3), CFE_SUCCESS);

    /* Initialize the message content */
    UtAssert_INT32_EQ(CFE_MSG_Init(&CmdMsg.CmdHeader.Msg, CFE_FT_CMD_MSGID, sizeof(CmdMsg)), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_MSG_Init(&TlmMsg.TlmHeader.Msg, CFE_FT_TLM_MSGID, sizeof(TlmMsg)), CFE_SUCCESS);

    CFE_MSG_SetSequenceCount(&CmdMsg.CmdHeader.Msg, 11);
    CFE_MSG_SetSequenceCount(&TlmMsg.TlmHeader.Msg, 21);

    /* Sending with sequence update should ignore the sequence in the msg struct */
    CmdMsg.CmdPayload = 0x0c0ffee;
    TlmMsg.TlmPayload = 0x0d00d1e;
    UtAssert_INT32_EQ(CFE_SB_TransmitMsg(&CmdMsg.CmdHeader.Msg, true), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_SB_TransmitMsg(&TlmMsg.TlmHeader.Msg, true), CFE_SUCCESS);

    CmdMsg.CmdPayload = 0x1c0ffee;
    TlmMsg.TlmPayload = 0x1d00d1e;
    UtAssert_INT32_EQ(CFE_SB_TransmitMsg(&CmdMsg.CmdHeader.Msg, true), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_SB_TransmitMsg(&TlmMsg.TlmHeader.Msg, true), CFE_SUCCESS);

    /* Sending without sequence update should use the sequence in the msg struct */
    CmdMsg.CmdPayload = 0x2c0ffee;
    TlmMsg.TlmPayload = 0x2d00d1e;
    UtAssert_INT32_EQ(CFE_SB_TransmitMsg(&CmdMsg.CmdHeader.Msg, false), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_SB_TransmitMsg(&TlmMsg.TlmHeader.Msg, false), CFE_SUCCESS);

    /* Sending again should trigger MsgLimit errors on the pipe, however the call still returns CFE_SUCCESS */
    CmdMsg.CmdPayload = 0x3c0ffee;
    TlmMsg.TlmPayload = 0x3d00d1e;
    UtAssert_INT32_EQ(CFE_SB_TransmitMsg(&CmdMsg.CmdHeader.Msg, true), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_SB_TransmitMsg(&TlmMsg.TlmHeader.Msg, true), CFE_SUCCESS);

    /* Attempt to send a msg which does not have a valid msgid  */
    memset(&CFE_FT_BigMsg, 0xFF, sizeof(CFE_FT_BigMsg));
    CFE_MSG_SetSize(&CFE_FT_BigMsg.Hdr, sizeof(CFE_MSG_Message_t) + 4);
    UtAssert_INT32_EQ(CFE_SB_TransmitMsg(&CFE_FT_BigMsg.Hdr, true), CFE_SB_BAD_ARGUMENT);

    /* Attempt to send a msg which is too big */
    CFE_MSG_SetSize(&CFE_FT_BigMsg.Hdr, sizeof(CFE_FT_BigMsg));
    CFE_MSG_SetMsgId(&CFE_FT_BigMsg.Hdr, CFE_FT_CMD_MSGID);
    UtAssert_INT32_EQ(CFE_SB_TransmitMsg(&CFE_FT_BigMsg.Hdr, true), CFE_SB_MSG_TOO_BIG);

    /* Attempt to send a msg which is NULL */
    UtAssert_INT32_EQ(CFE_SB_TransmitMsg(NULL, true), CFE_SB_BAD_ARGUMENT);

    UtPrintf("Testing: CFE_SB_ReceiveBuffer");

    /* off nominal / bad arguments */
    UtAssert_INT32_EQ(CFE_SB_ReceiveBuffer(&MsgBuf, CFE_SB_INVALID_PIPE, 100), CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_EQ(CFE_SB_ReceiveBuffer(NULL, PipeId1, 100), CFE_SB_BAD_ARGUMENT);
    UtAssert_INT32_EQ(CFE_SB_ReceiveBuffer(&MsgBuf, PipeId1, -100), CFE_SB_BAD_ARGUMENT);

    /*
     * Note, the CFE_SB_TransmitMsg ignores the "IncrementSequence" flag for commands.
     * Thus, all the sequence numbers should come back with the original value set (11)
     */
    UtAssert_INT32_EQ(CFE_SB_ReceiveBuffer(&MsgBuf, PipeId1, 100), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_MSG_GetMsgId(&MsgBuf->Msg, &MsgId), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_MSG_GetSequenceCount(&MsgBuf->Msg, &Seq1), CFE_SUCCESS);
    CFE_UtAssert_MSGID_EQ(MsgId, CFE_FT_CMD_MSGID);
    CmdPtr = (const CFE_FT_TestCmdMessage_t *)MsgBuf;
    UtAssert_UINT32_EQ(CmdPtr->CmdPayload, 0x0c0ffee);
    UtAssert_UINT32_EQ(Seq1, 11);

    UtAssert_INT32_EQ(CFE_SB_ReceiveBuffer(&MsgBuf, PipeId1, 100), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_MSG_GetMsgId(&MsgBuf->Msg, &MsgId), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_MSG_GetSequenceCount(&MsgBuf->Msg, &Seq1), CFE_SUCCESS);
    CFE_UtAssert_MSGID_EQ(MsgId, CFE_FT_CMD_MSGID);
    CmdPtr = (const CFE_FT_TestCmdMessage_t *)MsgBuf;
    UtAssert_UINT32_EQ(CmdPtr->CmdPayload, 0x1c0ffee);
    UtAssert_UINT32_EQ(Seq1, 11);

    UtAssert_INT32_EQ(CFE_SB_ReceiveBuffer(&MsgBuf, PipeId1, 100), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_MSG_GetMsgId(&MsgBuf->Msg, &MsgId), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_MSG_GetSequenceCount(&MsgBuf->Msg, &Seq1), CFE_SUCCESS);
    CFE_UtAssert_MSGID_EQ(MsgId, CFE_FT_CMD_MSGID);
    CmdPtr = (const CFE_FT_TestCmdMessage_t *)MsgBuf;
    UtAssert_UINT32_EQ(CmdPtr->CmdPayload, 0x2c0ffee);
    UtAssert_UINT32_EQ(Seq1, 11);

    /* Final should not be in the pipe, should have been rejected due to MsgLim */
    UtAssert_INT32_EQ(CFE_SB_ReceiveBuffer(&MsgBuf, PipeId1, 100), CFE_SB_TIME_OUT);

    /*
     * For TLM, the CFE_SB_TransmitMsg obeys the "IncrementSequence" flag.
     * Thus, first message gets the reference point, next message should be one more.
     */
    UtAssert_INT32_EQ(CFE_SB_ReceiveBuffer(&MsgBuf, PipeId2, 100), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_MSG_GetMsgId(&MsgBuf->Msg, &MsgId), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_MSG_GetSequenceCount(&MsgBuf->Msg, &Seq1), CFE_SUCCESS);
    CFE_UtAssert_MSGID_EQ(MsgId, CFE_FT_TLM_MSGID);
    TlmPtr = (const CFE_FT_TestTlmMessage_t *)MsgBuf;
    UtAssert_UINT32_EQ(TlmPtr->TlmPayload, 0x0d00d1e);

    UtAssert_INT32_EQ(CFE_SB_ReceiveBuffer(&MsgBuf, PipeId2, 100), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_MSG_GetMsgId(&MsgBuf->Msg, &MsgId), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_MSG_GetSequenceCount(&MsgBuf->Msg, &Seq2), CFE_SUCCESS);
    CFE_UtAssert_MSGID_EQ(MsgId, CFE_FT_TLM_MSGID);
    TlmPtr = (const CFE_FT_TestTlmMessage_t *)MsgBuf;
    UtAssert_UINT32_EQ(TlmPtr->TlmPayload, 0x1d00d1e);
    UtAssert_UINT32_EQ(Seq2, CFE_MSG_GetNextSequenceCount(Seq1));

    UtAssert_INT32_EQ(CFE_SB_ReceiveBuffer(&MsgBuf, PipeId2, 100), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_MSG_GetMsgId(&MsgBuf->Msg, &MsgId), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_MSG_GetSequenceCount(&MsgBuf->Msg, &Seq2), CFE_SUCCESS);
    CFE_UtAssert_MSGID_EQ(MsgId, CFE_FT_TLM_MSGID);
    TlmPtr = (const CFE_FT_TestTlmMessage_t *)MsgBuf;
    UtAssert_UINT32_EQ(TlmPtr->TlmPayload, 0x2d00d1e);
    UtAssert_UINT32_EQ(Seq2, 21);

    /* Final should not be in the pipe, should have been rejected due to MsgLim */
    UtAssert_INT32_EQ(CFE_SB_ReceiveBuffer(&MsgBuf, PipeId2, 100), CFE_SB_TIME_OUT);

    /* Cleanup */
    UtAssert_INT32_EQ(CFE_SB_DeletePipe(PipeId1), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_SB_DeletePipe(PipeId2), CFE_SUCCESS);
}

/* This is a variant of the message transmit API that does not copy */
void TestZeroCopyTransmitRecv(void)
{
    CFE_SB_PipeId_t  PipeId1;
    CFE_SB_PipeId_t  PipeId2;
    CFE_SB_Buffer_t *CmdBuf;
    CFE_SB_Buffer_t *TlmBuf;
    CFE_SB_Buffer_t *MsgBuf;
    CFE_SB_MsgId_t   MsgId;

    /* Setup, create a pipe and subscribe (one cmd, one tlm) */
    UtAssert_INT32_EQ(CFE_SB_CreatePipe(&PipeId1, 5, "TestPipe1"), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_SB_CreatePipe(&PipeId2, 5, "TestPipe2"), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_SB_SubscribeEx(CFE_FT_CMD_MSGID, PipeId1, CFE_SB_DEFAULT_QOS, 3), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_SB_SubscribeEx(CFE_FT_TLM_MSGID, PipeId2, CFE_SB_DEFAULT_QOS, 3), CFE_SUCCESS);

    UtPrintf("Testing: CFE_SB_AllocateMessageBuffer");

    /* Confirm bad size rejection */
    UtAssert_NULL(CFE_SB_AllocateMessageBuffer(CFE_MISSION_SB_MAX_SB_MSG_SIZE + 1));

    /* Nominal */
    UtAssert_NOT_NULL(CmdBuf = CFE_SB_AllocateMessageBuffer(sizeof(CFE_FT_TestCmdMessage_t)));
    UtAssert_NOT_NULL(TlmBuf = CFE_SB_AllocateMessageBuffer(sizeof(CFE_FT_TestTlmMessage_t)));

    UtPrintf("Testing: CFE_SB_ReleaseMessageBuffer");

    /* allocate a buffer but then discard it without sending */
    UtAssert_NOT_NULL(MsgBuf = CFE_SB_AllocateMessageBuffer(sizeof(CFE_MSG_Message_t) + 4));
    UtAssert_INT32_EQ(CFE_SB_ReleaseMessageBuffer(MsgBuf), CFE_SUCCESS);

    /* Attempt to double-release, should fail validation */
    UtAssert_INT32_EQ(CFE_SB_ReleaseMessageBuffer(MsgBuf), CFE_SB_BUFFER_INVALID);

    /* Other bad input checking */
    UtAssert_INT32_EQ(CFE_SB_ReleaseMessageBuffer(NULL), CFE_SB_BAD_ARGUMENT);

    UtPrintf("Testing: CFE_SB_TransmitBuffer");

    /* Initialize the message content */
    UtAssert_INT32_EQ(CFE_MSG_Init(&CmdBuf->Msg, CFE_FT_CMD_MSGID, sizeof(CFE_FT_TestCmdMessage_t)), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_MSG_Init(&TlmBuf->Msg, CFE_FT_TLM_MSGID, sizeof(CFE_FT_TestTlmMessage_t)), CFE_SUCCESS);

    UtAssert_INT32_EQ(CFE_SB_TransmitBuffer(CmdBuf, true), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_SB_TransmitBuffer(TlmBuf, true), CFE_SUCCESS);

    /* Attempt to send a buffer which has been released */
    UtAssert_NOT_NULL(MsgBuf = CFE_SB_AllocateMessageBuffer(sizeof(CFE_MSG_Message_t) + 4));
    UtAssert_INT32_EQ(CFE_MSG_Init(&MsgBuf->Msg, CFE_FT_CMD_MSGID, sizeof(CFE_MSG_Message_t) + 4), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_SB_ReleaseMessageBuffer(MsgBuf), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_SB_TransmitBuffer(MsgBuf, true), CFE_SB_BUFFER_INVALID);

    /* Attempt to send a NULL buffer */
    UtAssert_INT32_EQ(CFE_SB_TransmitBuffer(NULL, true), CFE_SB_BAD_ARGUMENT);

    UtPrintf("Testing: CFE_SB_ReceiveBuffer");

    UtAssert_INT32_EQ(CFE_SB_ReceiveBuffer(&MsgBuf, PipeId1, 100), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_MSG_GetMsgId(&MsgBuf->Msg, &MsgId), CFE_SUCCESS);
    CFE_UtAssert_MSGID_EQ(MsgId, CFE_FT_CMD_MSGID);
    UtAssert_ADDRESS_EQ(MsgBuf, CmdBuf); /* should be the same actual buffer (not a copy) */

    UtAssert_INT32_EQ(CFE_SB_ReceiveBuffer(&MsgBuf, PipeId1, 100), CFE_SB_TIME_OUT);

    UtAssert_INT32_EQ(CFE_SB_ReceiveBuffer(&MsgBuf, PipeId2, 100), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_MSG_GetMsgId(&MsgBuf->Msg, &MsgId), CFE_SUCCESS);
    CFE_UtAssert_MSGID_EQ(MsgId, CFE_FT_TLM_MSGID);
    UtAssert_ADDRESS_EQ(MsgBuf, TlmBuf); /* should be the same actual buffer (not a copy) */

    UtAssert_INT32_EQ(CFE_SB_ReceiveBuffer(&MsgBuf, PipeId2, 100), CFE_SB_TIME_OUT);

    /* Cleanup */
    UtAssert_INT32_EQ(CFE_SB_DeletePipe(PipeId1), CFE_SUCCESS);
    UtAssert_INT32_EQ(CFE_SB_DeletePipe(PipeId2), CFE_SUCCESS);
}

void SBSendRecvTestSetup(void)
{
    UtTest_Add(TestBasicTransmitRecv, NULL, NULL, "Test Basic Transmit/Receive");
    UtTest_Add(TestZeroCopyTransmitRecv, NULL, NULL, "Test Zero Copy Transmit/Receive");
}
