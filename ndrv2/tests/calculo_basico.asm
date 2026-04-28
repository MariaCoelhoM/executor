; Gerado automaticamente — compilador NASM v2
; Pipeline: .nasm -> lexer -> parser -> AST -> codegen -> .asm

        ORG  0

        ; _t0 + _t1
        LDA  _t0
        ADD  _t1
        STA  _t2
        ; _t2 / _t3
        LDA  _t3
        NOT
        ADD  _t7
        STA  _t8
        LDA  _t2
        STA  _t6
_La0:
        LDA  _t6
        ADD  _t8
        JN   _Lb0
        STA  _t6
        LDA  _t5
        ADD  _t7
        STA  _t5
        JMP  _La0
_Lb0:
        LDA  _t5
        STA  _t4
        ; guardar resultado em quociente
        LDA  _t4
        STA  quociente
        HLT

; ── secao de dados ──────────────────�
_t0            DATA 10
_t1            DATA 6
_t2            DATA 0
_t3            DATA 4
_t5            DATA 0
_t6            DATA 0
_t7            DATA 1
_t8            DATA 0
_t4            DATA 0
quociente      DATA 0
