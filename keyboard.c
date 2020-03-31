/* Mode */
#define INT_DISABLE           0b11000000
#define INT_ENABLE            0b01000000
#define IRQ_MODE              0b10010
#define SVC_MODE              0b10011
/* Memory */
#define A9_ONCHIP_END         0xFFFFFFFF
/* Cyclone V FPGA Device */
#define PS2_BASE              0xFF200100
/* Interrupt controller (GIC) CPU interface(s) */
#define MPCORE_GIC_CPUIF      0xFFFEC100    // PERIPH_BASE + 0x100
#define ICCICR                0x00          // offset to CPU interface control reg
#define ICCPMR                0x04          // offset to interrupt priority mask reg
/* Interrupt controller (GIC) distributor interface(s) */
#define MPCORE_GIC_DIST       0xFFFED000    // PERIPH_BASE + 0x1000
#define ICDDCR                0x00          // offset to distributor control reg
/* Keyboard related Variables */
#define PS2_IRQ 			  79            // Interrupt ID
#define PS2_L_ARROW           0x6B
#define PS2_R_ARROW           0x74
#define PS2_ENTER             0x5A


// configuring interrupts
void set_A9_IRQ_stack(); // initiate the stack pointer for IRQ mode
void config_GIC(); // configure the general interrupt controller
void config_PS2(); // configure PS2 to generate interrupts
void disable_A9_interrupts(); //turn off interrupts
void enable_A9_interrupts(); // enable interrupts
void config_interrupts(int N, int CPU_target);

// exception handler
void __attribute__((interrupt))__cs3_isr_irq();
void __attribute__((interrupt))__cs3_reset();
void __attribute__((interrupt))__cs3_isr_undef();
void __attribute__((interrupt))__cs3_isr_swi();
void __attribute__((interrupt))__cs3_isr_pabort();
void __attribute__((interrupt))__cs3_isr_dabort();
void __attribute__((interrupt))__cs3_isr_fiq();

// Interrupt Service Routine
void PS2_ISR();
void KEY_ISR();


void main(){
    disable_A9_interrupts();
    set_A9_IRQ_stack();
    config_GIC();
    config_PS2();
    enable_A9_interrupts();

    while(1){}

}


// ISR for keyboard
void PS2_ISR(){
    volatile int* PS2_ptr = (int *) PS2_BASE;
    
    volatile int* HEX_ptr = (int *) 0xFF200020;

    int PS2_data = *(PS2_ptr) & 0xFF;
    if (PS2_data == PS2_R_ARROW){
        *HEX_ptr = 0b00111111;
    } else if (PS2_data == PS2_L_ARROW){
        *HEX_ptr = 0b00000110;
    } else if (PS2_data == PS2_ENTER){
        *HEX_ptr = 0b01001111;
    } else {
        *HEX_ptr = 0b00000000;
    }

    return;
}


// IRQ exception handler
void __attribute__((interrupt))__cs3_isr_irq(){
    int interrupt_ID = *((int *) 0xFFFEC10C);

    if (interrupt_ID == PS2_IRQ){
        PS2_ISR();
    } else {
        while(1);
    }
    
    *((int*) 0xFFFEC110) = interrupt_ID;
}


// configure PS2 to reset & generate interrupts
void config_PS2(){
    volatile int* PS2_ptr = (int*) PS2_BASE;

    *(PS2_ptr) = 0xFF; // reset
    volatile int* PS2_ptr_ctrl = (int*) 0xFF200104;
    *(PS2_ptr_ctrl) = 1; // set RE to 1 to generate interrupts
}


// initiate the stack pointer for IRQ mode
// code taken from DE1-SOC ARM Cortex A9 Document
void set_A9_IRQ_stack(){
    int stack, mode;
    stack = A9_ONCHIP_END - 7;

    // change mode
    mode = INT_DISABLE | IRQ_MODE;
    asm("msr cpsr, %[ps]" : : [ps] "r"(mode));
    // set banked stack pointer
    asm("mov sp, %[ps]" : : [ps] "r"(stack));

    // go back to SVC mode
    mode = INT_DISABLE | SVC_MODE;
    asm("msr cpsr, %[ps]" : : [ps] "r"(mode));
}


// turn off interrupts in the ARM processor
void disable_A9_interrupts(){
    int status = INT_DISABLE | SVC_MODE;
    asm("msr cpsr, %[ps]" : : [ps] "r"(status));
}


// turn on interrupts in the ARM processor
void enable_A9_interrupts(){
    int status = INT_ENABLE | SVC_MODE;
    asm("msr cpsr, %[ps]" : : [ps] "r"(status));
}


// configure the Generic Interrupt Controller GIC
// code taken from DE1-SOC ARM Cortex A9 Document
void config_GIC(){
    int address;

    config_interrupts(PS2_IRQ, 1);

    // set ICCPMR
    address = MPCORE_GIC_CPUIF + ICCPMR;
    *((int*) address) = 0xFFFF;

    // set CPU ICCICR
    address = MPCORE_GIC_CPUIF + ICCICR;
    *((int*) address) = 1;

    // configure ICDDCR
    address = MPCORE_GIC_DIST + ICDDCR;
    *((int*) address) = 1;

}


// configure ICDISERn and ICDIPTRn
// code taken from Using the ARM GIC document
void config_interrupts(int N, int CPU_target){
    int reg_offset, index, value, address;

    reg_offset = (N >> 3) & 0xFFFFFFFC;
    index = N & 0x1F;
    value = 0x1 << index;
    address = 0xFFFED100 + reg_offset;

    *(int*) address |= value;

    reg_offset = (N & 0xFFFFFFFC);
    index = N & 0x3;
    address = 0xFFFED800 + reg_offset + index;

    *(char *) address = (char)CPU_target;
}