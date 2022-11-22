; RUN: opt -passes=module-inline -inline-priority-mode=top-down --debug -S < %s 2>&1 | FileCheck %s

; Call graph:
;   A->C<-B
;      |   
;      v  
;      D
;      |
;      v
;      E

define void @A() {
    call void @C()
    ret void
}
define internal void @B() {
    call void @C()
    ret void
}
define internal void @C() {
    call void @D()
    ret void
}
define internal void @D() {
    call void @E()
    ret void
}
define internal void @E() {
    ret void
}

; Since we're inlining top-down A and B are the only valid callers
; CHECK-NOT: Analyzing call of {{C|D|E}}... (caller:{{C|D}})