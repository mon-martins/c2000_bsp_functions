/*
 * temp_ipc.c
 *
 *  Created on: 15 de mar de 2024
 *      Author: ramon.martins
 */

#include "bsp_ipc.h"


#define IPC_ADDR_OFFSET_CORR(addr, corr)  (((addr) * (corr)) / 2U)

#ifndef IPC_REGISTER_CMD
#define IPC_REGISTER_CMD 0xFFFFFF01
#endif

#ifndef IPC_REGISTER_FLAG
#define IPC_REGISTER_FLAG IPC_FLAG31
#endif


#pragma DATA_SECTION(BSP_IPC_CPU1_CM_Data,   "MSGRAM_CM_TO_CPU1")
#pragma DATA_SECTION(BSP_IPC_CPU2_CM_Data,   "MSGRAM_CM_TO_CPU2")

BSP_IPC_CPU1_CM_Data_t BSP_IPC_CPU1_CM_Data;
BSP_IPC_CPU2_CM_Data_t BSP_IPC_CPU2_CM_Data;

BSP_IPC_PutBuffer_t* const BSP_IPC_PutInstance[IPC_TOTAL_NUM] = {
     /* IPC_CM_L_CPU1_R */
    BSP_IPC_CPU1_CM_Data.PutBuffer,
    BSP_IPC_CPU2_CM_Data.PutBuffer,
};

uint16_t IPC_CPU1xCM_Interrupt_RSV[8]   = {0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF} ;
uint16_t IPC_CPU2xCM_Interrupt_RSV[8]   = {0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF} ;

//*****************************************************************************
//
// IPCinitMessageQueue
//
//*****************************************************************************
void BSP_IPC_initMessageQueue(
        IPC_Type_t ipcType, volatile BSP_IPC_MessageQueue_t *msgQueue,
        uint32_t ipcInt_L, uint32_t ipcInt_R)
{
    //
    // Check for arguments
    //
    uint16_t PutBufferOffset = 0;

    ASSERT(msgQueue != NULL);
    ASSERT(ipcInt_L < 8);
    ASSERT(ipcInt_R < 8);

    if(ipcType == 0){
        // IPC Type CPU1_to_CM

        for(uint16_t i=0; i<8;i++){
            if(IPC_CPU1xCM_Interrupt_RSV[i] != 0xFFFF) PutBufferOffset++;
        }
        ASSERT(PutBufferOffset < BSP_IPC_INTERRUPTS_CPU1_CM_USED);
        ASSERT(IPC_CPU1xCM_Interrupt_RSV[ipcInt_L] == 0xFFFF);
        IPC_CPU1xCM_Interrupt_RSV[ipcInt_L] = PutBufferOffset;

    }else{
        // IPC Type CPU2_to_CM

        for(uint16_t i=0; i<8;i++){
            if(IPC_CPU2xCM_Interrupt_RSV[i] != 0xFFFF) PutBufferOffset++;
        }
        ASSERT(PutBufferOffset < BSP_IPC_INTERRUPTS_CPU2_CM_USED);
        ASSERT(IPC_CPU2xCM_Interrupt_RSV[ipcInt_L] == 0xFFFF);
        IPC_CPU2xCM_Interrupt_RSV[ipcInt_L] = PutBufferOffset;
    }

    msgQueue->ipcType = ipcType;

    BSP_IPC_PutBuffer_t *putBuffer = BSP_IPC_PutInstance[ipcType];
    putBuffer = putBuffer+PutBufferOffset;

    //
    // L->R Put Buffer and Index Initialization
    //
    msgQueue->PutBuffer     = putBuffer;

    IPC_sendCommand(ipcType, IPC_REGISTER_FLAG, IPC_ADDR_CORRECTION_ENABLE, IPC_REGISTER_CMD,(uint32_t) putBuffer, ipcInt_L);
    IPC_waitForFlag(ipcType, IPC_REGISTER_FLAG);

    uint32_t command;
    uint32_t data;
    BSP_IPC_GetBuffer_t *getBuffer;

    IPC_readCommand(ipcType, IPC_REGISTER_FLAG, IPC_ADDR_CORRECTION_ENABLE, &command,(uint32_t*) &getBuffer, &data);

    // The cores must configure IPC at the same time
    ASSERT(command == IPC_REGISTER_CMD);

    // Compatibility with the other core
    ASSERT(data == ipcInt_R);
    msgQueue->PutFlag       = (uint32_t)1U << ipcInt_R;

    //
    // L->R Get Buffer and Index Initialization
    //
    msgQueue->GetBuffer     = getBuffer;

    //
    // Initialize PutBuffer WriteIndex = 0 and GetBuffer ReadIndex = 0
    //
    msgQueue->PutBuffer->GetReadIndex = 0U;
    msgQueue->PutBuffer->PutWriteIndex  = 0U;

    IPC_ackFlagRtoL(ipcType, IPC_REGISTER_FLAG);
    IPC_waitForAck(ipcType, IPC_REGISTER_FLAG);
}

//*****************************************************************************
//
// IPC_sendMessageToQueue
//
//*****************************************************************************
bool BSP_IPC_sendMessageToQueue(
        volatile BSP_IPC_MessageQueue_t *msgQueue,
        BSP_IPC_Message_t *msg, bool addrCorrEnable, bool block)
{

    //
    // Check for arguments
    //
    ASSERT(msgQueue != NULL);
    ASSERT(msg != NULL);


    IPC_Type_t ipcType = msgQueue->ipcType;
    uint16_t writeIndex;
    uint16_t readIndex;
    uint16_t ret = true;

    writeIndex = msgQueue->PutBuffer->PutWriteIndex;
    readIndex  = msgQueue->GetBuffer->PutReadIndex;

    //
    // Wait until Put Buffer slot is free
    //
    while(((writeIndex + 1U) % BSP_IPC_BUFFER_SIZE) == readIndex)
    {
        //
        // If designated as a "Blocking" function, and Put buffer is full,
        // return immediately with fail status.
        //
        if(!block)
        {
            ret = false;
            break;
        }

        readIndex = msgQueue->GetBuffer->PutReadIndex;
    }

    if(ret != false)
    {
        //
        // When slot is free, Write Message to PutBuffer, update PutWriteIndex,
        // and set the CPU IPC INT Flag
        //
        msgQueue->PutBuffer->Buffer[writeIndex] = *msg;

#if BSP_USE_POINTER_IPC_MQ
        if(addrCorrEnable)
        {
            msgQueue->PutBuffer->Buffer[writeIndex].address -=
                IPC_Instance[ipcType].IPC_MsgRam_LtoR;
        }
#endif

        writeIndex = (writeIndex + 1U) % BSP_IPC_BUFFER_SIZE;
        msgQueue->PutBuffer->PutWriteIndex = writeIndex;

        IPC_setFlagLtoR(ipcType, msgQueue->PutFlag);
    }

    return(ret);
}

//*****************************************************************************
//
// IPC_readMessageFromQueue
//
//*****************************************************************************
bool BSP_IPC_readMessageFromQueue(
        volatile BSP_IPC_MessageQueue_t *msgQueue,
        BSP_IPC_Message_t *msg, bool addrCorrEnable, bool block)
{
    //
    // Check for arguments
    //
    ASSERT(msgQueue != NULL);
    ASSERT(msg != NULL);

    uint16_t writeIndex;
    uint16_t readIndex;
    uint16_t ret = true;
    IPC_Type_t ipcType = msgQueue->ipcType;

    writeIndex = msgQueue->GetBuffer->GetWriteIndex;
    readIndex  = msgQueue->PutBuffer->GetReadIndex;

    //
    // Loop while GetBuffer is empty
    //
    while(writeIndex == readIndex)
    {
        //
        // If designated as a "Blocking" function, and Get buffer is empty,
        // return immediately with fail status.
        //
        if(!block)
        {
            ret = false;
            break;
        }

        writeIndex = msgQueue->GetBuffer->GetWriteIndex;
    }

    if(ret != false)
    {
        //
        // If there is a message in GetBuffer, Read Message and update
        // the ReadIndex
        //
        *msg = msgQueue->GetBuffer->Buffer[readIndex];
#if BSP_USE_POINTER_IPC_MQ
        if(addrCorrEnable)
        {
            msg->address = IPC_Instance[ipcType].IPC_MsgRam_RtoL +
                           IPC_ADDR_OFFSET_CORR(msg->address,
                                        IPC_Instance[ipcType].IPC_Offset_Corr);
        }
#endif

        readIndex = (readIndex + 1U) % BSP_IPC_BUFFER_SIZE;
        msgQueue->PutBuffer->GetReadIndex = readIndex;
    }

    return(ret);
}
