# samu_monitor
为树莓派Raspberry Pi Pico开发的模仿了1970年代末早期风格的内存管理程序
(但由于pico硬件限制 本程序无法像当年的内存管理程序一样，需要Pico ROM 内的BootLoader启动跳转至位于Flash后运行)
目前版本，执行机器码部分没有使用原生执行RAM指令，而是使用内置精简的虚拟机指令
0x00 NOP
0x01 HALT
0x02 LDA addr
0x03 STA addr
0x04 LDA imm
0x05 ADD imm
0x06 JMP addr
0x07 JZ addr
0x08 OUT
0x09 
Monitor支持：
1.读取某一内存 e.g. 0000
2.读取某一范围内存 e.g. 0000.00FF
3.写入内存 e.g. 0000 FF 12 56
4.执行内存(由内置虚拟机执行) e.g. 0000R
