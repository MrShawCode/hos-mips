.set noreorder
.text
.globl _u_syscall_intr
.type _u_syscall_intr, @function
.ent _u_syscall_intr
_u_syscall_intr:
# push stack, saves
addiu $sp, $sp, -20
sw $a0, 0($sp)
sw $a1, 4($sp)
sw $a2, 8($sp)
sw $a3, 12($sp)
sw $t0, 20($sp)
# save and change stack pointer,
# as we reloads a0 with syscall parameter
sw $sp, 24($a0)
move $sp, $a0
# read arguments
lw $v0, 0($sp)
lw $a0, 4($sp)
lw $a1, 8($sp)
lw $a2, 12($sp)
lw $a3, 16($sp)
lw $t0, 20($sp)

syscall

# restore stack pointer first
lw $a0, 24($sp)
move $sp, $a0

lw $a0, 0($sp)
lw $a1, 4($sp)
lw $a2, 8($sp)
lw $a3, 12($sp)
lw $t0, 20($sp)
addiu $sp, $sp, 20

jr $ra
nop

.end _u_syscall_intr