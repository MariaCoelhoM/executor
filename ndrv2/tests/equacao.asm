; Gerado automaticamente — compilador NASM v2
; Pipeline: .nasm -> lexer -> parser -> AST -> codegen -> .asm

        ORG  0

        ; _t1 + _t2
        LDA  _t1
        ADD  _t2
        STA  _t3
        ; _t0 * _t3
        LDA  _t3
        STA  _t6
_La0:
        LDA  _t6
        JZ   _Lb0
        LDA  _t5
        ADD  _t0
        STA  _t5
        LDA  _t6
        ADD  _t7
        STA  _t6
        JMP  _La0
_Lb0:
        LDA  _t5
        STA  _t4
        ; _t4 - _t8
        LDA  _t8
        NOT
        ADD  _t10
        STA  _t11
        LDA  _t4
        ADD  _t11
        STA  _t9
        ; guardar resultado em saida
        LDA  _t9
        STA  saida
        HLT

; ── secao de dados ──────────────────�
_t0            DATA 2
_t1            DATA 3
_t2            DATA 4
_t3            DATA 0
_t5            DATA 0
_t6            DATA 0
_t7            DATA 255
_t4            DATA 0
_t8            DATA 5
_t10           DATA 1
_t11           DATA 0
_t9            DATA 0
saida          DATA 0
