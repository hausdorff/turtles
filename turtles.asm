;;; Apple //e Lisp
;;; Copyright (c) 2013, Alex Clemmer and Martin Törnwall
;;;
;;; This program is free software distributed under the terms of the
;;; MIT license. See the enclosed LICENSE file for more information.

;;; 
;;; SOFTWARE STACK:
;;; 
;;; The software stack starts high in memory (configurable) and grows downward.
;;; It should be used for pushing pointer-sized (16 bits) values only.
;;; The software stack is scanned conservatively when determining the GC
;;; root set, so anything on the stack that looks like a pointer is treated
;;; as such.
;;; 
;;; HEAP:
;;;
;;; The heap stores LISP objects. It starts at a "low" address and grows
;;; upwards towards the top of the stack. Heap and stack overflows are
;;; trivially checked by comparing the two pointers. Unlike the stack, the
;;; heap is not automatically managed; as objects become unreachable, a
;;; garbage collector kicks in and cleans them up.
;;;
;;; Every object on the heap is tagged with a one-byte "type tag".
;;; Currently the following types are recognized:
;;;   - Pairs (cons cells, to form lists)
;;;   - Integers
;;;   - Symbols (interned, to allow "eq?"-type comparisons)
;;; 
;;; APPROXIMATE MEMORY MAP:
;;;
;;; +-------+ $FFFF
;;; |  ROM  |
;;; +-------+
;;; | sstk  | Software stack. Grows down towards the heap.
;;; +-------+
;;; |       |
;;; | heap  | LISP heap. Grows up towards the software stack.
;;; |       |
;;; +-------+
;;; | LISP  |
;;; | intrp |
;;; +-------+
;;; |  txt  | Text-mode page 1. Used for text output.
;;; +-------+
;;; | hwstk | Hardware stack (fixed at page $01)
;;; +-------+
;;; |  ZP   | Zero-page used as scratchpad and pointer storage.
;;; +-------+ $0000
;;;
;;; CALLING CONVENTION:
;;;
;;; Procedure calls shall not clobber the X and Y registers. The A register is
;;; used for the return value and is thus not preserved across calls. It is
;;; always the callee's responsibility to preserve X and Y. The special
;;; RET location on the ZP is used to return 16-bit results.
;;;
;;; Parameters are passed on the software stack in reverse order.
;;; The callee is responsible for popping arguments of the stack,
;;; meaning that SSP_before_call = SSP_after_call.

SSP  = $00 ;; Software stack pointer (two locations)
HEAP = $02 ;; Heap pointer (two locations)
RET  = $04 ;; 16-bit return "register" (two locations)

* = $0800

INIT:
    ;; Initialize software stack at $BFFE (I/O space starts at $C000)
    LDA #$FF : STA SSP   ; SSP_low  = $FE
    LDA #$BF : STA SSP+1 ; SSP_high = $BF
    ;; Initialize the heap pointer at 
    LDA #<CODE_END : STA HEAP   ; HEAP_low = end of code low
    LDA #>CODE_END : STA HEAP+1 ; HEAP_high = end of code high
    ;; TODO Initialize symbol hash table.
    ;; TODO Set up global symbols (quote, lambda and define)
    ;; TODO Initialize the global environment.
    BRK

;; GC() -> NONE
GC:
    RTS

;; MAYBE_GC(SIZE :: BYTE) -> NONE
MAYBE_GC:
    RTS

;; MAKE_PAIR(CAR :: PTR, CDR :: PTR) -> PTR
MAKE_PAIR:
    ;; Do garbage collection prior to allocation as needed.
    LDA #5 : JSR S_PUSH_A
    JSR MAYBE_GC

;; S_PUSH_A() -> NONE
;; Pushes the A register twice onto the stack as the software
;; stack is required to be aligned on a 16-bit boundary.
S_PUSH_A:
    TYA : PHA
    LDY #0
    STA (SSP),Y
    INY
    STA (SSP),Y
    PLA : TAY
    JSR S_DEC_SP
    RTS

;; S_POP_A() -> BYTE
;; Pops the A register from the software stack. Assumes it
;; was pushed by S_PUSH_A, which actually pushes A twice to
;; maintain stack alignment.
S_POP_A:
    TYA : PHA
    LDY #1
    LDA (SSP),Y
    PHA
    JSR S_INC_SP
    PLA
    PLA : TAY
    RTS

;; S_INC_SP() -> NONE
;; Increments the software stack pointer by two.
S_INC_SP:
                LDA SSP
                CLC
                ADC #2          ; NOTE could optimize with 2xINC
                STA SSP
                BCC S_INC_SP_TMP ; If ZF is set then low byte addition wrapped
                INC SSP+1
S_INC_SP_TMP    RTS

;; S_DEC_SP() -> NONE
;; Decrements the software stack pointer two.
S_DEC_SP:
                LDA SSP
                SEC
                SBC #2          ; NOTE could optimize with 2xDEC
                STA SSP
                BCC S_DEC_SP_TMP ; If NF is set, low byte subtraction wrapped
                DEC SSP+1
S_DEC_SP_TMP    RTS

;; Marks the end of the code section; used to determine heap start.
;; MUST be after all code!
CODE_END
