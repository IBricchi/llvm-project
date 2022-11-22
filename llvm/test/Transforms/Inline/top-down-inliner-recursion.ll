; RUN: opt -passes=module-inline -inline-priority-mode=top-down --debug -S < %s 2>&1 | FileCheck %s

; Call graph:
;      A
;      |
;      v
;      B<-\ 
;      |  |
;      v  |
;      C--/

define void @A() {
    call void @B();
    ret void
}
define internal void @B() {
    call void @C()
    ret void
}
define internal void @C() {
    call void @B()
    ret void
}

; Check the top part of the call graph
; CHECK: Analyzing call of B... (caller:A)
; CHECK: Analyzing call of C... (caller:A)
; Failed recursive call
; CHECK: Inlining calls in: A 
; The inlining order in the SCC may be arbitrary
; CHECK: Analyzing call of {{B|C}}... (caller:{{B|C}})
; CHECK: Analyzing call of {{B|C}}... (caller:{{B|C}})
; CHECK: Analyzing call of {{B|C}}... (caller:{{B|C}})