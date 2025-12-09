# OS-Development <br> (Smart Photo Frame with AFTx07)
## Introduction
This project integrates SLIP, FatFs, CRC, and VGA to devlop a Smart Photo Frame that receives images sent from an external device to the AFTx07 and displays them on a monitor in a slideshow format.

## Image Requirement
|  **Property**  | **Specification** |
|----------------|-------------------|
| Resolution     | 640 x 480         |
| File Format    | BMP               |
| Color Format   | RGB565            |

## Circuit Requirement
### SD and SPI
For SD and SPI setup, please refer to the README in the [fatfs-aft](https://github.com/Purdue-SoCET/fatfs-aft).
### UART
Because the SLIP protocol's sender and receiver must be able to communicate with each other, two FTDIs are required for UART. Please refer to the table below for the connection setup.
|FTDI1 TX|FTDI1 RX|FTDI2 TX|FTDI2 RX|
|--------|--------|--------|--------|
|PinX    |PinX    |PinX    |PinX    |

## Hardware Requirement
### Implement VGA controller to the AFTx07
Seongbin's part
### Implement CRC accelerator to the AFTx07
Daeun's part

## Software Requirement
* Ensure that all files in this repository are placed in a single folder on the ASICFAB server.
* Use your personal PC as the external device for sending images.
* Ensure that `mx07_sender.py` (in the SLIP folder) and the image you want to send are located in the same directory on your PC.

## General Flow
### 1. Start program (Flash to FPGA)
To flash the program onto the FPGA: 
   
**1.** Load the Quartus and RISC-V GCC modules.
<pre>ml intel/quartus-std
ml riscv-gcc</pre>

**2.** Build the project.
<pre>make</pre>

**3.** Generate the initialization file.
<pre>make fpgainit.mif</pre>

**4.** Flash the program to the FPGA.
<pre>make fpga_flash</pre>

Once flashing is complete, `main.c` start running automatically on the FPGA.

### 2. Choose image and send it using UART
Prepare the sender environment:
* Place your image file and x07_sender.py in the same directory.
* Refer to command.txt for the transmission command format.

Example: sending test.bmp
<pre>python x07_sender.py --port /dev/tty.usbserial-A50285BI --baud 9600 --file test.bmp --chunk 1024</pre>
> [!NOTE]  
> The serial port depends on the FTDI device connected to your system.  
> Check the FTDI model and specify the correct port.

During transmission, the terminal will display logs such as the number of bytes sent and `"sent data"`.

### 3. Receive image and display on a monitor
Because of the image size, transmission typically takes about 10 minutes.

After the FPGA finishes receiving the data:
* A completion message will appear in the terminal.
* The image will be displayed on the monitor with a wave-motion effect.

### 4. Slide Show
If additional images are sent, repeat the same transsmion steps.

When **two or more images** are stored and no active transfer is in progress, the FPGA automatically cycles through them, displyaing each image **like a slide show**.

## Flow of Read/Write Operation
### Receive image and store in SD card
Attach flowchart diagram here
### Open image data and send them to VGA controller
Attach flowchart diagram here