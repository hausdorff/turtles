;;; Bastardized use of the `cpp` utility to expand out common addresses
#define SPACE   #$20
#define LETTERA #$41
#define LETTERZ #$5B
#define ZERO    #$30
#define NINE    #$39
#define LPAREN  #$40
#define RPAREN  #$41

#define TNIL    #$00
#define TNUMBER #$01
#define TSYMBOL #$02
#define TPAIR   #$03

#define HEAP  $00
#define HEAPH $01

#define HOOP  $02
#define HOOPH $03

#define SCRATCH $04

INIT:   LDA #$00
        STA HEAP
        STA HOOP
        STA HOOPH
        LDA #$80
        STA HEAPH
        JMP REPL

;;; Read-Eval-Print Loop
REPL:   JSR PROMPT
        JSR READ
        LDA #$0A
        JSR PRINT
        JSR WRITE
        JMP REPL

;;; Print prompt ("> ")
PROMPT: LDA #$3E
        JSR PRINT
        LDA #$20
        JSR PRINT
        RTS

;;; Read a single character of input.
GETCH:  LDA $C000
        BPL GETCH               ; bad character; try again
        EOR #$80                ; clear top bit to get the ASCII code
        JSR PRINT               ; echo to console
        STA $C010               ; acknowledge the read
        RTS

;;; Print a single character to the output.
PRINT:  JSR $FDF0               ; print it
        RTS

;;; Write a Lisp object in readable form.
;;; Assumes X:Y holds a pointer to an object.
WRITE:  STX HOOPH               ; jump through hoops...
        LDA (HOOP),Y            ; load type
        INY                     ; skip type
        CMP TSYMBOL
        BEQ WRSYM
        CMP TNUMBER
        BEQ WRNUM
        ;; XXX: more types
        ;; just fail for now
        BRK

WRSYM:  LDA (HOOP),Y            ; load next char
        INY
        CMP #$00                ; null terminator reached?
        BEQ WRITEND
        JSR PRINT               ; output char
        JMP WRSYM               ; next char
        JMP WRITEND

WRNUM:  LDA (HOOP),Y            ; load value
        JSR PRINT               ; just print as a byte for now
        JMP WRITEND

WRITEND:
        RTS

;;; Read a single Lisp datum.
;;; Returns a heap object in X:Y
READ:
        ;; skip leading spaces
        JSR GETCH               ; read next char
        CMP SPACE
        BEQ READ

        ;; looking at a letter?
        CMP LETTERA
        BPL RDSYM               ; XXX check upper bound (Z)!

        ;; looking at a number?
        CMP ZERO
        BPL RDNUM               ; XXX check upper bound (9)!

        ;; XXX handle error (nothing matched)
        BRK

RDSYM:
        ;; XXX this block is a duplicate of the block in RDNUM!
        LDY #$00
        PHA                     ; push A
        LDA TSYMBOL
        STA (HEAP),Y            ; *heap = symbol type
        PLA                     ; restore A
        INY                     ; heap++

RDSYML: STA (HEAP),Y            ; *heap = current char
        INY                     ; heap++
        JSR GETCH               ; read next char
        CMP LETTERA             ; >= A ?
        BMI RDSYMS              ; last alphabetic char?
        JMP RDSYML              ; next char
RDSYMS: LDA #$0
        STA (HEAP),Y            ; *heap = '\0'
        INY                     ; heap++
        JSR HEAPALLOC
        JMP READEND

RDNUM:
        ;; XXX this block is a duplicate of the block in RDSYM!
        LDY #$00
        PHA                     ; push A
        LDA TNUMBER
        STA (HEAP),Y
        INY
        PLA
        CLC
        SBC ZERO                ; make number from ASCII

;;; XXX: Using uninitialized heap space! May be nonzero!
RDNUML: STA (HEAP),Y            ; save result of *10/initial value
        JSR GETCH
        CMP ZERO
        BMI RDNUMS
        CMP NINE
        BEQ RDNUMC              ; continue if == 9
        BPL RDNUMS              ; stop if > 9
RDNUMC: CLC                     ; make sure carry is clear
        SBC ZERO                ; make number from ASCII
        PHA                     ; save A
        LDA (HEAP),Y
        ASL A                   ; times two ...
        ASL A                   ; times two ...
        ASL A                   ; times two = times 8
        ADC (HEAP),Y            ; plus A ...
        ADC (HEAP),Y            ; plus A = times 10 total
        PLA                     ; restore A
        ADC (HEAP),Y            ; add the new number
        JMP RDNUML

RDNUMS:
        INY                     ; we actually allocated two bytes
        JSR HEAPALLOC
        JMP READEND

READEND:
        RTS

;;; Reserve Y bytes of heap space. Set X:Y to the heap location before
;;; the allocation was performed.
HEAPALLOC:
        ;; reusable heap reservation code--increments the heap pointer by
        ;; the amount of space we allocated for the symbol name
        LDX HEAPH               ; X = heap high byte before allocation
        TYA                     ; A = allocated heap space so far
        LDY HEAP                ; Y = heap low byte before allocation
        CLC                     ; make sure carry is clear
        ADC HEAP
        STA HEAP
        LDA HEAPH
        ADC #$00
        STA HEAPH
        RTS
