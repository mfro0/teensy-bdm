/*
 * File:        sysinit.h
 * Purpose:     Kinetis Configuration
 *              Initializes processor to a default state
 *
 * Notes:
 *
 */

/********************************************************************/

// function prototypes
extern void sysinit(void);
extern void trace_clk_init(void);
extern void fb_clk_init(void);
extern int32_t pll_init(int8_t prdiv_val, int8_t vdiv_val);
extern void wdog_disable(void);

/********************************************************************/
