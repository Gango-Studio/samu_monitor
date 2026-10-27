#include "pico/stdlib.h"
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <cctype>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <cctype>
#include <cstdint>

const size_t MEMORY_SIZE = 64*1024;//virtual RAM of 64KB

uint8_t memory[MEMORY_SIZE] = {0}; // Initialize memory with zeros

typedef struct{
    uint8_t add;
    uint16_t pc_cnt;
    uint8_t zero;
    bool is_halted;
} CPU_8samu;

CPU_8samu cpu;

void show_range(uint32_t start , uint32_t end){
    if( start >= 64*1024 || end >= 64*1024){
        printf("Address Out Of Range\n");
        return ;
    }
    if(start > end){
        uint32_t tmp = start;
        start = end;
        end = tmp;
    }
    for (uint32_t addr = start; addr <= end; addr += 16){
        uint32_t count = end - addr +1;
        if (count > 16) count = 16;
        printf("0x%04X:",addr);
        for(uint32_t i = 0; i < count;i++){
            printf("%02X ",memory[addr + i]);
        }
        for(uint32_t i = count; i < 16;i++){
            printf("   ");
        }
        printf("\r\n");
    }
    return ;
}

void run(uint32_t addr_run){
    
    cpu.pc_cnt = addr_run;
    cpu.is_halted = false;
    cpu.zero = 0;
    cpu.add = 0;
    int step_count = 0;
    const int max_steps = 1000; // Limit the number of steps to prevent infinite loops
    while(!cpu.is_halted && step_count < max_steps){
        step_count++;
        if(cpu.pc_cnt >= MEMORY_SIZE){
            printf("Program Counter Out Of Range\n");
            break;
        }
        uint8_t opcode = memory[cpu.pc_cnt++];
        // Implement the instruction execution logic here
        // For demonstration, let's just increment the program counter
        switch(opcode){
            case 0x00:{// NOP
                break;
            }
            case 0x01: {// HALT
                cpu.is_halted = true;
                break;
            }
            case 0x02: {// LDA addr
                uint8_t addrH = memory[cpu.pc_cnt];
                uint8_t addrL = memory[cpu.pc_cnt + 1];
                uint16_t target_addr = (addrH << 8) | addrL;
                cpu.add = memory[target_addr];
                cpu.zero = (cpu.add == 0) ? 1 : 0;
                cpu.pc_cnt += 2; // Move past the address bytes
                break;
            }
            case 0x03:{//STA
                memory[(memory[cpu.pc_cnt] << 8) | memory[cpu.pc_cnt + 1]] = cpu.add;
                cpu.pc_cnt += 2; // Move past the address bytes
                break;
            }
            case 0x04:{//LDA imm
                uint8_t imm = memory[cpu.pc_cnt];
                cpu.add = imm;
                cpu.zero = (cpu.add == 0) ? 1:0;
                cpu.pc_cnt ++;
                break;
            }
            case 0x05:{//ADD
                uint8_t add_imm = memory[cpu.pc_cnt];
                cpu.add += add_imm;
                cpu.zero = (cpu.add == 0) ? 1:0;
                cpu.pc_cnt++;
                break;
            }
            case 0x06:{//JMP
                uint8_t addrH = memory[cpu.pc_cnt];
                uint8_t addrL = memory[cpu.pc_cnt + 1];
                uint16_t target_addr = (addrH << 8) | addrL;
                cpu.pc_cnt = target_addr;
                break;
            }
            case 0x07:{//JZ
                uint8_t addrH = memory[cpu.pc_cnt];
                uint8_t addrL = memory[cpu.pc_cnt + 1];
                uint16_t target_addr = (addrH << 8) | addrL;
                if(cpu.zero == 1){cpu.pc_cnt = target_addr;}
                else{cpu.pc_cnt += 2;}
                break;
            }
            case 0x08:{//OUT
                printf("%c",cpu.add);
                fflush(stdout);
                break;
            }
            default:{
                printf("Unknown opcode: 0x%02X at address 0x%04X\n", opcode, cpu.pc_cnt - 1);
                cpu.is_halted = true; // Halt on unknown opcode
                break;
            }
        }
    }
    return ;
}

// ===== 以下为 Pico 适配代码，原有逻辑未改动 =====
// Pico 上没有可用的 stdin 行缓冲，用 USB 串口逐字符读一行来替代 fgets
static bool read_line(char* buf, size_t cap){
    size_t n = 0;
    for(;;){
        int c = getchar_timeout_us(0);
        if(c == PICO_ERROR_TIMEOUT){
            tight_loop_contents();
            continue;
        }
        if(c == '\r' || c == '\n'){
            if(n == 0) continue;
            buf[n] = '\0';
            printf("\r\n");
            return true;
        }
        if(c == 8 || c == 127){
            if(n > 0){ n--; printf("\b \b"); fflush(stdout); }
            continue;
        }
        if(c >= 32 && c < 127){
            if(n + 1 < cap){ buf[n++] = (char)c; putchar(c); fflush(stdout); }
        }
    }
}
int main(){
    stdio_init_all();
    stdio_usb_init();   // Pico 适配：启动 USB 串口

    printf("=====Samu Monitor=====\n");
    char cmdline[256];
    while(read_line(cmdline, sizeof(cmdline))) {
        cmdline[strcspn(cmdline, "\r\n")] = '\0'; // Remove newline character
        if(strlen(cmdline) == 0) {
            continue; // Skip empty lines
        }
        size_t len = strlen(cmdline);
        if(len > 0 && (cmdline[len - 1] == 'R' || cmdline[len - 1] == 'r')) {
            cmdline[len - 1] = '\0'; 
            uint32_t address = strtoul(cmdline,NULL,16);
            run(address);
            continue;
        }
        char* check_dot = strchr(cmdline, '.');
        if(check_dot != NULL){
            *check_dot = '\0';
            uint32_t start_addr = strtoul(cmdline,NULL,16);
            uint32_t end_addr = strtoul(check_dot + 1,NULL,16);
            show_range(start_addr,end_addr);
            continue;
        }
       char* token = strtok(cmdline," \t");
       if(token == NULL) continue;
       // Analysis address
       char* suffix;
       unsigned long addr_val = strtoul(token,&suffix,16);
       if(suffix == token || *suffix != '\0'){
        printf("Invalid address\n");
        continue;
       }
       uint32_t addr_single = (uint32_t)addr_val;
       if(addr_single > 0xFFFF){
        printf("Out Of Range 0xffff");
        continue;
       }
       uint8_t data[256];
       int cnt = 0;
       bool is_valid = true;
       token = strtok(NULL," \t");
       while(token != NULL){
            char* dend;
            unsigned long opt_addr = strtoul(token,&dend,16);
            if(dend == token || *dend != '\0'){
                    printf("Invalid Data");
                    is_valid = false;
                    break;
            }
            if(opt_addr > 0xFF){
                    printf("Out Of Range 0xffff");
                    is_valid = false;
                    break;
            }
            if(cnt >= 256){
                    printf("Maximum of Bytes is 256");
                    is_valid = false;
                    break;
            }

            data[cnt++] = (uint8_t) opt_addr;
            token = strtok(NULL," \t");
        }
        if(!is_valid) continue;
        if(cnt == 0){
                printf("0x%04X: 0x%02X\n",addr_single,memory[addr_single]);
                continue;
        }
        if(addr_single + cnt > 64*1024){
                printf("Data Out Of Range\n");
                continue;
        }
        for(int i = 0; i < cnt;i++){
            memory[addr_single + i] = data[i];
        }
    
    }
    return 0;
}