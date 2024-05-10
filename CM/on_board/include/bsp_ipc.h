/*
 * BSP_ipc.h
 *
 *  Created on: 15 de mar de 2024
 *      Author: ramon.martins
 */

#ifndef BSP_DRIVERLIB_INCLUDE_BSP_IPC_H_
#define BSP_DRIVERLIB_INCLUDE_BSP_IPC_H_

#include "ipc_config.h"
#include "cm.h"

#if BSP_MQ_SUPPORT


#if !defined(CM)
#error "You must define CM in your project properties"
#endif

#if ( defined(CPU1) ||  defined(CPU2))
#error "This Code is for the CM"
#endif

#if (!(BSP_IPC_MAX_PAYLOAD_SIZE >= 1) && BSP_USE_PAYLOAD_IPC_MQ)
#error "You must define the maximum size of your payload(in 16bits) on BSP_IPC_MAX_PAYLOAD_SIZE"
#endif

#if !BSP_USE_COMMAND_IPC_MQ && !BSP_USE_POINTER_IPC_MQ && !BSP_USE_PAYLOAD_IPC_MQ
#error "You must define the type of data, that will be used in MQ "
#endif

#if !(BSP_IPC_BUFFER_SIZE>1)
#error "You must define the size of the buffer and it has to be larger than 1, if possible one larger than you need"
#endif

#if !BSP_IPC_INTERRUPTS_CPU1_CPU2_USED && !BSP_IPC_INTERRUPTS_CPU1_CM_USED && !BSP_IPC_INTERRUPTS_CPU2_CM_USED
#error "You must define how many interrupts you will use in each IPC"
#endif

typedef struct
{
#if BSP_USE_COMMAND_IPC_MQ
    uint32_t command;
#endif
#if BSP_USE_POINTER_IPC_MQ
    uint32_t address;
#endif
#if BSP_USE_PAYLOAD_IPC_MQ
    uint16_t Payload[BSP_IPC_MAX_PAYLOAD_SIZE];
#endif
}BSP_IPC_Message_t;

typedef struct
{
    BSP_IPC_Message_t  Buffer[BSP_IPC_BUFFER_SIZE];
    uint16_t            PutWriteIndex;
    uint16_t            GetReadIndex;
}BSP_IPC_PutBuffer_t;

typedef struct
{
    BSP_IPC_Message_t  Buffer[BSP_IPC_BUFFER_SIZE];
    uint16_t            GetWriteIndex;
    uint16_t            PutReadIndex;
}BSP_IPC_GetBuffer_t;

typedef struct
{
    BSP_IPC_PutBuffer_t *  PutBuffer;
    volatile BSP_IPC_GetBuffer_t *  GetBuffer;
    uint32_t         PutFlag;
    IPC_Type_t ipcType;
} BSP_IPC_MessageQueue_t;

typedef struct
{
    BSP_IPC_PutBuffer_t PutBuffer[BSP_IPC_INTERRUPTS_CPU1_CPU2_USED];
} BSP_IPC_CPU1_CPU2_Data_t;

typedef struct
{
    BSP_IPC_PutBuffer_t PutBuffer[BSP_IPC_INTERRUPTS_CPU1_CM_USED];
} BSP_IPC_CPU1_CM_Data_t;

typedef struct
{
    BSP_IPC_PutBuffer_t PutBuffer[BSP_IPC_INTERRUPTS_CPU2_CM_USED];
} BSP_IPC_CPU2_CM_Data_t;


void BSP_IPC_initMessageQueue(
        IPC_Type_t ipcType, volatile BSP_IPC_MessageQueue_t *msgQueue,
        uint32_t ipcInt_L, uint32_t ipcInt_R);

bool BSP_IPC_sendMessageToQueue(
        volatile BSP_IPC_MessageQueue_t *msgQueue,
        BSP_IPC_Message_t *msg, bool addrCorrEnable, bool block);

bool BSP_IPC_readMessageFromQueue(
        volatile BSP_IPC_MessageQueue_t *msgQueue,
        BSP_IPC_Message_t *msg, bool addrCorrEnable, bool block);

#endif

#endif /* BSP_DRIVERLIB_INCLUDE_BSP_IPC_H_ */
