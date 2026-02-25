# Stack trace generation code
# This code can be included in the generated assembly to provide runtime stack traces

.section .text
.global print_stack_trace
print_stack_trace:
    push %rbp
    mov %rsp, %rbp
    
    # Save registers
    push %rax
    push %rbx
    push %rcx
    push %rdx
    
    # Print stack trace header
    lea stack_trace_header(%rip), %rdi
    call printf
    
    # Walk the stack
    mov %rbp, %rbx  # Current frame pointer
    xor %rcx, %rcx  # Frame counter
    
stack_walk_loop:
    cmp $10, %rcx    # Limit to 10 frames
    jge stack_walk_done
    
    test %rbx, %rbx  # Check if frame pointer is valid
    jz stack_walk_done
    
    # Get return address
    mov 8(%rbx), %rax
    
    # Print frame info
    lea stack_frame_format(%rip), %rdi
    mov %rcx, %rsi
    mov %rax, %rdx
    call printf
    
    # Move to next frame
    mov (%rbx), %rbx
    inc %rcx
    jmp stack_walk_loop
    
stack_walk_done:
    # Restore registers
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax
    
    pop %rbp
    ret
    
.section .rodata
stack_trace_header:
    .string "Stack trace:\n"
stack_frame_format:
    .string "  #%zu: 0x%zx\n"
