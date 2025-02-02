
void foo(void)  __arm_streaming_compatible {
  asm(".arch armv9-a+sme2");
  asm("smstart");
  asm("ldr zt0, [sp]");
}

