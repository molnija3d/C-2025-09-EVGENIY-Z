    bits 64
    extern malloc, puts, printf, fflush, abort, free
    global main

    section   .data
empty_str: db 0x0
int_format: db "%ld ", 0x0
data: dq 4, 8, 15, 16, 23, 42
data_length: equ ($-data) / 8

section   .text
;;; print_int proc
print_int: ;print int value
    push rbp
    mov rbp, rsp
    sub rsp, 16

    mov rsi, rdi;second param - value
    mov rdi, int_format ;first param - "ld "
    xor rax, rax ;zero rax (printf output)
    call printf

    xor rdi, rdi ;zero rdi (first param)
    call fflush ;flush terminal buffer

    mov rsp, rbp
    pop rbp
    ret

;;; p proc
p: ;predicate proc
    mov rax, rdi
    and rax, 1 ;parity check
    ret

;;; add_element proc
add_element:
    push rbp
    push rbx
    push r14
    mov rbp, rsp
    sub rsp, 16

    mov r14, rdi ;data
    mov rbx, rsi ;pointer to next 

    mov rdi, 16  ;size of structure
    call malloc
    test rax, rax
    jz abort ;abort if malloc failed

    mov [rax], r14 ;store data to element value
    mov [rax + 8], rbx ;store next pointer

    mov rsp, rbp
    pop r14
    pop rbx
    pop rbp
    ret

;;; m proc
m:  ;apply func to every element of the list. Mapping function
    ;rdi pointer to list
    ;rsi pointer to print function
    ;loop version instead of recursion
    push rbp ;save rbp
    push rbx ;save rbx
    mov rbp, rsi ;save pointer to function
    mov rbx, rdi ;save pointer to list
 
 loopmf:
    test rbx,rbx ;if zero, end loop
    jz endmf
    mov rdi, [rbx] ;first param - load value of the list node
    call rbp ;call function
    mov rbx, [rbx+8] ; load list->next to rdi (first param)
    jmp loopmf;
endmf:
    pop rbx ;restore rbx
    pop rbp ;restore rbp
    ret

;;; f proc - ITERATIVE VERSION (filter function)
f:
    push rbx
    push r12
    push r13
    
    mov rbx, rdi               ; rbx = current node in input list
    mov r12, rsi               ; r12 = accumulator (head of result list)
    mov r13, rdx               ; r13 = predicate function
    
    xor rcx, rcx               ; rcx = tail of result list (NULL initially)
    
.filter_loop:
    test rbx, rbx              ; check if end of input list
    jz .filter_done
    
    ; Test current value with predicate
    mov rdi, [rbx]             ; get current value
    call r13                   ; call predicate
    test rax, rax              ; check result
    jz .next_node
    
    ; Value matches predicate - add to result list
    mov rdi, [rbx]             ; value to add
    mov rsi, r12             ; second param - rsi (pointer to new list)
    call add_element           ; create new node
    mov r12, rax         ; tail->next = new node

.next_node:
    mov rbx, [rbx + 8]         ; move to next node in input list
    jmp .filter_loop
    
.filter_done:
    mov rax, r12               ; return head of result list
    pop r13
    pop r12
    pop rbx
    ret

;;; free_list proc
free_list:
    test rdi, rdi ;check if list is not zero
    jz .end_free

    push rdi ;save pointer to list
    mov rdi, [rdi + 8] ;move to rdi list->next
    call free_list ;recurisvely call free_list
    pop rdi ;restore rdi (pointer to list)
 
    call free ; free list

.end_free:
    ret
;;; main proc
main:
    mov rbp, rsp; for correct debugging
    push rbx

    xor rax, rax
    mov rbx, data_length
adding_loop:
    mov rdi, [data - 8 + rbx * 8] ;first param - data
    mov rsi, rax ;second param - pointer to list
    call add_element
    dec rbx
    jnz adding_loop

    mov rbx, rax ;store pointer to list in rbx

    mov rdi, rax ;pointer to list - first param
    mov rsi, print_int ;pointer to print function - second param
    call m ;apply print_int to every element of the list

    mov rdi, empty_str
    call puts ; puts ""

    mov rdx, p   ;third param - pointer to p()
    xor rsi, rsi ;second paream - zero
    mov rdi, rbx ;first param - pointer to list
    call f
    mov r12, rax ;store pointer to list in r12 

    mov rdi, rax ;first param - pointer to filtered list
    mov rsi, print_int ;sedond param - pointer to print_int
    call m ;apply print_int to every element in the list

    mov rdi, empty_str
    call puts ;puts empty string

    mov rdi, rbx ;first param - pointer to list
    call free_list
    mov rdi, r12 ;first param - pinter to filtered list
    call free_list

    pop rbx


    xor rax, rax ;zeroing
    ret
