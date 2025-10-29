# Compiler and flags
CC = riscv64-unknown-elf-gcc
CFLAGS = -g -march=rv32imc_zicsr_zifencei -mabi=ilp32 -mcmodel=medany -Os
LDFLAGS = -specs=${HOME}/opt/picolibc/aftx07/lib/picolibc.specs -Taftx07.ld -Wl,-gc-sections

# AFTx07 sim
SIM_PATH = ../aft_out/socet_aft_aftx07_2.0.0/sim-verilator/Vaftx07
CFLAGS += -DUART_STDIO

# Source files
FATFS = FatFs/source/*.c
#SRCS = FatFs/image_test.c FatFs/os.c FatFs/image.c $(FATFS)
SRCS = main.c FatFs/os.c $(FATFS)
#SRCS = vga_test.c FatFs/os.c $(FATFS)

# Output files
TARGET = a.out
BIN = meminit.bin
FPGA_TARGET = fpga.out
FPGA_BIN = fpga.bin
FPGA_MIF = fpgainit.mif

# Default target
all: $(TARGET) $(BIN)

# Compile and link
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

# Generate binary from ELF
$(BIN): $(TARGET)
	riscv64-unknown-elf-objcopy -O binary $< $@

# Generate objdump
objdump: $(TARGET)
	riscv64-unknown-elf-objdump -d $< > objdump.txt

objdump_debug: $(TARGET)
	riscv64-unknown-elf-objdump -dS $< > objdump.txt

# Generate map file
map: $(TARGET)
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=meminit.map $(SRCS) -o $(TARGET)

# Clean up
clean:
	rm -f $(TARGET) meminit.map $(BIN) objdump.txt fpgainit.mif memsim.hex

#flashes the bin to the fpga
$(FPGA_MIF): $(BIN)
	python3 ../bin_to_mif.py

fpga_compile:
	cd .. && \
	quartus_sh -t fpga_compile.tcl

fpga_program:
	cd .. && \
	quartus_sh -t fpga_program.tcl

fpga_flash: $(FPGA_MIF)
	make fpga_program
	cp fpgainit.mif ../
	cd .. && \
	quartus_stp -t fpga_flash.tcl
	rm ../fpgainit.mif

sim_xcelium: $(BIN)
	cp meminit.bin ../aft_out_xcelium/socet_aft_aftx07_2.0.0/sim-xcelium/meminit.bin
	cd ../aft_out_xcelium/socet_aft_aftx07_2.0.0/sim-xcelium/ && make run-gui

sim: $(BIN)
	$(SIM_PATH)

sim_uart: $(BIN)
	$(SIM_PATH) --uart

.PHONY: all clean objdump map fpga sim
