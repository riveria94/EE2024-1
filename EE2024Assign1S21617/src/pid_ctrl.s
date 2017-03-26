 	.syntax unified
 	.cpu cortex-m3
 	.thumb
 	.align 2
 	.global	pid_ctrl
 	.thumb_func

 	.equ Kp, 105
 	.equ Ki, 10
 	.equ Kd, 80

@  EE2024 Assignment 1: pid_ctrl(int en, int st) assembly language function
@  CK Tham, ECE, NUS, 2017

@  Name: Cheng Yong Quan
@  Matric No.: A0138952W

@  Name: Andrew Sugiarto
@  Matric No.: A0139872R

pid_ctrl:

	PUSH {R2-R9}

@	Data Library
@ 	R0 = en
@ 	R1 = st
@ 	R2 = enOld
@ 	R3 = sn
@ 	R4 = upper bound of sn
@   R5 = lower bound of sn
@ 	R6 = address of enOld
@   R7 = Kp + Kd
@   R8 = Ki
@   R9 = Kd

    LDR R6, =enOld
    LDM R6, {R2-R3}
    LDR R4, =#950
    LDR R5, =#-950
    MOV R7, Kp
    MOV R8, Ki
    MOV R9, Kd

@ 	If start, initialise sn and enOld to 0
    TEQ R1, #1
    ITT EQ
    MOVEQ R2, #0
    MOVEQ R3, #0

@ 	sn = sn + en
    ADD R3, R3, R0

@	If sn exceeds upper boundary, set to upper boundary
    CMP R3, R4
    IT GT
    MOVGT R3, R4

@	If sn exceeds lower boundary, set to lower boundary
    CMP R3, R5
    IT LT
    MOVLT R3, R5

@   Store sn and enOld in memory
    STM R6, {R0, R3}

@	un = Kp*en + Ki*sn + Kd*(en-enOld) = (Kp+Kd)*en + Ki*sn - Kd*enOld
    MUL R0, R7, R0
    MLA R0, R8, R3, R0
    MLS R0, R9, R2, R0

	POP	{R2-R9}
 	BX	LR

    .lcomm enOld 4
    .lcomm sn 4

