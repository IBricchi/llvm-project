; RUN: opt -passes=module-inline -inline-priority-mode=top-down --debug -S < %s 2>&1 | FileCheck %s

; Call graph:
;   /--A--\
;   |     |
;   v     v
;   B->D<-C
;      |   
;      v  
;      E

define void @A() {
    call void @B()
    call void @C()
    ret void
}
define internal void @B() {
    call void @D()
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

; Since we're inlining top-down A is the only valid caller
; CHECK-NOT: Analyzing call of {{B|C|D|E}}... (caller:{{B|C|D}})