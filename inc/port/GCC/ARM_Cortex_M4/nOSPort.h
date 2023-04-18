/*
 * Copyright (c) 2014-2019 Jim Tremblay
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef NOSPORT_H
#define NOSPORT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t                            nOS_Stack;
typedef uint32_t                            nOS_StatusReg;

typedef struct
{
    uint32_t RBAR;   // Region base address
    uint32_t RASR;   // Region attributes (type, region size, enable, etc.)
} nOS_MPU_Region;

typedef bool (*faultCallback)();

typedef struct
{
    nOS_MPU_Region    MPU_Table[8][2];          // RBAR and RASR entries
    faultCallback     callback;                // NULL pointer if no callback
} nOS_MPU_ProcessTbl;

#define NOS_UNUSED(v)                       (void)v

#define NOS_MEM_ALIGNMENT                   4
#define NOS_MEM_POINTER_WIDTH               4

#define NOS_32_BITS_SCHEDULER
#define NOS_USE_CLZ

#ifndef NOS_CONFIG_ISR_STACK_SIZE
 #error "nOSConfig.h: NOS_CONFIG_ISR_STACK_SIZE is not defined: must be higher than 0."
#elif (NOS_CONFIG_ISR_STACK_SIZE == 0)
 #error "nOSConfig.h: NOS_CONFIG_ISR_STACK_SIZE is set to invalid value: must be higher than 0."
#endif

/* __NVIC_PRIO_BITS defined from CMSIS if used */
#ifdef NOS_CONFIG_NVIC_PRIO_BITS
 #define NOS_NVIC_PRIO_BITS                 NOS_CONFIG_NVIC_PRIO_BITS
#elif defined(__NVIC_PRIO_BITS)
 #define NOS_NVIC_PRIO_BITS                 __NVIC_PRIO_BITS
#else
 #define NOS_NVIC_PRIO_BITS                 4
#endif

#ifdef NOS_CONFIG_MAX_UNSAFE_ISR_PRIO
 #if (NOS_CONFIG_MAX_UNSAFE_ISR_PRIO > 0)
  #define NOS_MAX_UNSAFE_BASEPRI            (NOS_CONFIG_MAX_UNSAFE_ISR_PRIO << (8 - NOS_NVIC_PRIO_BITS))
 #else
  #undef NOS_NVIC_PRIO_BITS
  #undef NOS_CONFIG_MAX_UNSAFE_ISR_PRIO
 #endif
#endif

__attribute__( ( always_inline ) ) static inline uint32_t _GetMSP (void)
{
    register uint32_t r;
    __asm volatile ("MRS %0, MSP" : "=r" (r));
    return r;
}

__attribute__( ( always_inline ) ) static inline void _SetMSP (uint32_t r)
{
    __asm volatile ("MSR MSP, %0" :: "r" (r));
}

__attribute__( ( always_inline ) ) static inline uint32_t _GetPSP (void)
{
    register uint32_t r;
    __asm volatile ("MRS %0, PSP" : "=r" (r));
    return r;
}

__attribute__( ( always_inline ) ) static inline void _SetPSP (uint32_t r)
{
    __asm volatile ("MSR PSP, %0" :: "r" (r));
}

__attribute__( ( always_inline ) ) static inline uint32_t _GetCONTROL (void)
{
    register uint32_t r;
    __asm volatile ("MRS %0, CONTROL" : "=r" (r));
    return r;
}

__attribute__( ( always_inline ) ) static inline void _SetCONTROL (uint32_t r)
{
    __asm volatile ("MSR CONTROL, %0" :: "r" (r));
}

__attribute__( ( always_inline ) ) static inline uint32_t _GetBASEPRI (void)
{
    register uint32_t r;
    __asm volatile ("MRS %0, BASEPRI" : "=r" (r));
    return r;
}

__attribute__( ( always_inline ) ) static inline void _SetBASEPRI (uint32_t r)
{
    __asm volatile ("MSR BASEPRI, %0" :: "r" (r));
}

__attribute__( ( always_inline ) ) static inline uint32_t _GetPRIMASK (void)
{
    uint32_t r;
    __asm volatile ("MRS %0, PRIMASK" : "=r" (r));
    return r;
}

__attribute__( ( always_inline ) ) static inline void _SetPRIMASK (uint32_t r)
{
    __asm volatile ("MSR PRIMASK, %0" :: "r" (r));
}

__attribute__( ( always_inline ) ) static inline void _DSB (void)
{
    __asm volatile ("DSB");
}

__attribute__( ( always_inline ) ) static inline void _ISB (void)
{
    __asm volatile ("ISB");
}

__attribute__( ( always_inline ) ) static inline void _NOP (void)
{
    __asm volatile ("NOP");
}

__attribute__( ( always_inline ) ) static inline void _DI (void)
{
    __asm volatile ("CPSID I");
}

__attribute__( ( always_inline ) ) static inline void _EI (void)
{
    __asm volatile ("CPSIE I");
}

__attribute__( ( always_inline ) ) static inline uint32_t _CLZ(uint32_t n)
{
    register uint32_t r;
    __asm volatile (
        "CLZ    %[result], %[input]"
        : [result] "=r" (r)
        : [input] "r" (n)
    );
    return r;
}

__attribute__( ( always_inline ) ) static inline void nOS_SetMPU_Regions (nOS_MPU_ProcessTbl* pTableRegion)
{
    // Only change privileged if task is not privileged
    if(pTableRegion != NULL)
    {
        __asm volatile (
            "PUSH   {R4-R12}                        \n"
            /* MPU->RBAR */

            "LDR    R4,         =0xE000ED9C         \n"
            /* Make sure outstanding transfers are done */
            "DMB    0x0F                            \n"
            /* Disable the MPU */
            "MOVS   R5,         #0                  \n"
            "STR    R5,         [R4,#-8]            \n"

        /* Update the first 8 MPU (v7M and V8M) */
            /* Read 8 words from the process table */
            "LDMIA %0!,         {R5-R12}            \n"
            //"LDMIA  %0!,        {R2-R9}             \n"
            /* Write 8 words to the MPU */
            "STMIA  R4,         {R5-R12}            \n"
            /* Read the next 8 words from the process table */
            "LDMIA  %0!,        {R5-R12}            \n"
            /* Write those 8 words to the MPU */
            "STMIA  R4,         {R5-R12}            \n"

        /* Write the next 8 MPU regions (v8M only) */
            /* Read 8 words from the process table */
            "LDMIA  %0!,        {R5-R12}            \n"
            /* Write 8 words to the MPU */
            "STMIA  R4,         {R5-R12}            \n"
            /* Read the next 8 words from the process table */
            "LDMIA  %0!,        {R5-R12}            \n"
            /* Write those 8 words to the MPU */
            "STMIA  R4,         {R5-R12}            \n"

            /* Memory barriers to ensure subsequent data + instruction */
            "DSB    0x0F                            \n"
            /* Transfer using updated MPU settings */
            "ISB    0x0F                            \n"
            /* Enable the MPU (assumes PRIVDEFENA is 1) */
            "MOVS   R5,         #5                  \n"
            "STR    R5,         [R4, #-8]           \n"

            "POP    {R4-R12}                        \n"
            "BX     LR                              \n"

            ".align 4                               \n"
            :: "r" (pTableRegion)
        );
    }
}

#ifdef NOS_MAX_UNSAFE_BASEPRI
#define nOS_EnterCritical(sr)                                                   \
    do {                                                                        \
        sr = _GetBASEPRI();                                                     \
        if (sr == 0 || sr > NOS_MAX_UNSAFE_BASEPRI) {                           \
            _SetBASEPRI(NOS_MAX_UNSAFE_BASEPRI);                                \
            _DSB();                                                             \
            _ISB();                                                             \
        }                                                                       \
    } while (0)

#define nOS_LeaveCritical(sr)                                                   \
    do {                                                                        \
        _SetBASEPRI(sr);                                                        \
        _DSB();                                                                 \
        _ISB();                                                                 \
    } while (0)

#define nOS_PeekCritical()                                                      \
    do {                                                                        \
        _SetBASEPRI(0);                                                         \
        _DSB();                                                                 \
        _ISB();                                                                 \
                                                                                \
        _NOP();                                                                 \
                                                                                \
        _SetBASEPRI(NOS_MAX_UNSAFE_BASEPRI);                                    \
        _DSB();                                                                 \
        _ISB();                                                                 \
    } while (0)
#else
#define nOS_EnterCritical(sr)                                                   \
    do {                                                                        \
        sr = _GetPRIMASK();                                                     \
        _DI();                                                                  \
        _DSB();                                                                 \
        _ISB();                                                                 \
    } while (0)

#define nOS_LeaveCritical(sr)                                                   \
    do {                                                                        \
        _SetPRIMASK(sr);                                                        \
        _DSB();                                                                 \
        _ISB();                                                                 \
    } while (0)

#define nOS_PeekCritical()                                                      \
    do {                                                                        \
        _EI();                                                                  \
        _DSB();                                                                 \
        _ISB();                                                                 \
                                                                                \
        _NOP();                                                                 \
                                                                                \
        _DI();                                                                  \
        _DSB();                                                                 \
        _ISB();                                                                 \
    } while (0)
#endif

void    nOS_EnterISR    (void);
void    nOS_LeaveISR    (void);

#define NOS_ISR(func)                                                           \
void func##_ISR(void) __attribute__ ( ( always_inline ) );                      \
void func(void)                                                                 \
{                                                                               \
    nOS_EnterISR();                                                             \
    func##_ISR();                                                               \
    nOS_LeaveISR();                                                             \
}                                                                               \
inline void func##_ISR(void)

#ifdef NOS_PRIVATE
 void       nOS_InitSpecific        (void);
 void       nOS_InitContext         (nOS_Thread *thread, nOS_Stack *stack, size_t ssize, nOS_ThreadEntry entry, void *arg);
 void       nOS_SwitchContext       (void);
 void       nOS_SetMPU_Regions      (nOS_MPU_ProcessTbl *MPU_ProcessTable);
#endif

#ifdef __cplusplus
}
#endif

#endif /* NOSPORT_H */
