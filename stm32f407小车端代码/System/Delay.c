#include "Delay.h"

static u32 fac_us=0;
static u32 fac_ms=0;

// SysTick LOAD寄存器是24位，最大值为0xFFFFFF = 16777215
#define SYSTICK_LOAD_MAX    0xFFFFFFUL

void Delay_Init(u8 SYSCLK)
{
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);
    fac_us=SYSCLK/8;
    fac_ms=fac_us*1000;
}

void Delay_us(u32 nus)
{
    u32 temp;
    u32 total_ticks = nus * fac_us;
    
    // 分多次调用，避免SysTick LOAD寄存器溢出（24位限制）
    while(total_ticks > SYSTICK_LOAD_MAX)
    {
        SysTick->LOAD = SYSTICK_LOAD_MAX;
        SysTick->VAL = 0x00;
        SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
        do
        {
            temp = SysTick->CTRL;
        }while((temp & 0x01) && !(temp & (1 << 16)));
        SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
        SysTick->VAL = 0X00;
        total_ticks -= SYSTICK_LOAD_MAX;
    }
    
    if(total_ticks > 0)
    {
        SysTick->LOAD = total_ticks;
        SysTick->VAL = 0x00;
        SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
        do
        {
            temp = SysTick->CTRL;
        }while((temp & 0x01) && !(temp & (1 << 16)));
        SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
        SysTick->VAL = 0X00;
    }
}

void Delay_ms(u16 nms)
{
    u32 temp;
    u32 total_ticks = (u32)nms * fac_ms;
    
    // 分多次调用，避免SysTick LOAD寄存器溢出（24位限制）
    while(total_ticks > SYSTICK_LOAD_MAX)
    {
        SysTick->LOAD = SYSTICK_LOAD_MAX;
        SysTick->VAL = 0x00;
        SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
        do
        {
            temp = SysTick->CTRL;
        }while((temp & 0x01) && !(temp & (1 << 16)));
        SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
        SysTick->VAL = 0X00;
        total_ticks -= SYSTICK_LOAD_MAX;
    }
    
    if(total_ticks > 0)
    {
        SysTick->LOAD = total_ticks;
        SysTick->VAL = 0x00;
        SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
        do
        {
            temp = SysTick->CTRL;
        }while((temp & 0x01) && !(temp & (1 << 16)));
        SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
        SysTick->VAL = 0X00;
    }
}
