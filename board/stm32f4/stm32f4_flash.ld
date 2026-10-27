/*
*****************************************************************************
**
**  File        : stm32f4_flash.ld
**
**  Abstract    : Linker script for STM32F413 Device with
**                1024KByte FLASH, 320KByte RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used.
**
**  Target      : STMicroelectronics STM32
**
**  Environment : Atollic TrueSTUDIO(R)
**
**  Distribution: The file is distributed "as is," without any warranty
**                of any kind.
**
**  (c)Copyright Atollic AB.
**  You may use this file as-is or modify it according to the needs of your
**  project. Distribution of this file (unmodified or modified) is not
**  permitted. Atollic AB permit registered Atollic TrueSTUDIO(R) users the
**  rights to distribute the assembled, compiled & linked contents of this
**  file as part of an application binary file, provided that it is built
**  using the Atollic TrueSTUDIO(R) toolchain.
**
*****************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
/* cookies at the top of SRAM, kept across resets and the startup RAM test */
enter_bootloader_mode = 0x2004FFFC;
fault_cookie = 0x2004FFF8;
self_test_cookie = 0x2004FFF4;

_estack = 0x2004FFF0;    /* end of 320K RAM minus the cookies */
_sram_start = ORIGIN(RAM);
_sram_end = ORIGIN(RAM) + LENGTH(RAM);
_app_start = 0x08004000; /* Reserve Sector 0(16K) for bootloader */

/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0x200;      /* required amount of heap  */
_Min_Stack_Size = 0x400; /* required amount of stack */

/* Specify the memory areas */
MEMORY
{
  FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 1024K
  RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 320K /* SRAM1(256K) + SRAM2(64K), contiguous */
}

/* Define output sections */
SECTIONS
{
  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >FLASH

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
	*(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  /* Constant data goes into FLASH */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH


   .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
    .ARM : {
    __exidx_start = .;
      *(.ARM.exidx*)
      __exidx_end = .;
    } >FLASH

  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(.fini_array*))
    KEEP (*(SORT(.fini_array.*)))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */

    /* safety state, one MPU region, see board/drivers/mpu.h */
    . = ALIGN(64);
    _ssafety = .;
    *(.data.controls_allowed .bss.controls_allowed)
    *(.data.current_safety_mode .bss.current_safety_mode)
    *(.data.current_safety_param .bss.current_safety_param)
    *(.data.current_hooks .bss.current_hooks)
    *(.data.current_safety_config .bss.current_safety_config)
    . = ALIGN(64);
    _esafety = .;

    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >RAM AT> FLASH

  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss secion */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = ALIGN(512);
    _stack_guard = .;
    . = . + 512;
    _stack_guard_end = .;
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  ASSERT((_esafety - _ssafety) <= 64, "safety state doesn't fit its MPU region")
  ASSERT(DEFINED(controls_allowed) ? (controls_allowed >= _ssafety && controls_allowed < _esafety) : !DEFINED(safety_tx_hook), "controls_allowed not in the MPU region")
  ASSERT(DEFINED(current_safety_mode) ? (current_safety_mode >= _ssafety && current_safety_mode < _esafety) : !DEFINED(safety_tx_hook), "current_safety_mode not in the MPU region")
  ASSERT(DEFINED(current_safety_param) ? (current_safety_param >= _ssafety && current_safety_param < _esafety) : !DEFINED(safety_tx_hook), "current_safety_param not in the MPU region")
  ASSERT(DEFINED(current_safety_config) ? (current_safety_config >= _ssafety && current_safety_config < _esafety) : !DEFINED(safety_tx_hook), "current_safety_config not in the MPU region")
  ASSERT(_estack <= _sram_end, "stack top outside SRAM")

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
