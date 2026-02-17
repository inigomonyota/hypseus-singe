/*!
\file    serialib.h
\brief   Header file of the class serialib. This class is used for communication over a serial device.
\author  Philippe Lucidarme (University of Angers)
\version 2.0
\date    december the 27th of 2019
This Serial library is used to communicate through serial port.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

This is a licence-free software, it can be used by anyone who try to build a better world.

-------------------------------------------------------------------------------
Windows note (MSVC):
- <sys/time.h> / timeval / gettimeofday are POSIX. Do not include them on Windows.
- timeOut uses QueryPerformanceCounter on Windows (monotonic, high-resolution).
-------------------------------------------------------------------------------
*/

#ifndef SERIALIB_H
#define SERIALIB_H

// Standard headers
#include <stdint.h>

// To avoid unused parameters
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

// Platform includes
#if defined(_WIN32) || defined(_WIN64)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#else

    // Unix-like platforms
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <string.h>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

// Some platforms also used sys/shm.h in the original header; keep it only where present/needed.
#if defined(__linux__) || defined(__APPLE__)
#include <sys/shm.h>
#endif

#endif


/**
 * number of serial data bits
 */
enum SerialDataBits {
    SERIAL_DATABITS_5,   /**< 5 databits */
    SERIAL_DATABITS_6,   /**< 6 databits */
    SERIAL_DATABITS_7,   /**< 7 databits */
    SERIAL_DATABITS_8,   /**< 8 databits */
    SERIAL_DATABITS_16,  /**< 16 databits */
};

/**
 * number of serial stop bits
 */
enum SerialStopBits {
    SERIAL_STOPBITS_1,     /**< 1 stop bit */
    SERIAL_STOPBITS_1_5,   /**< 1.5 stop bits */
    SERIAL_STOPBITS_2,     /**< 2 stop bits */
};

/**
 * type of serial parity bits
 */
enum SerialParity {
    SERIAL_PARITY_NONE,   /**< no parity bit */
    SERIAL_PARITY_EVEN,   /**< even parity bit */
    SERIAL_PARITY_ODD,    /**< odd parity bit */
    SERIAL_PARITY_MARK,   /**< mark parity */
    SERIAL_PARITY_SPACE   /**< space bit */
};


/*!  \class     serialib
     \brief     This class is used for communication over a serial device.
*/
class serialib {
public:
    //_____________________________________
    // ::: Constructors and destructors :::

    serialib();
    ~serialib();

    //_________________________________________
    // ::: Configuration and initialization :::

    bool SupportedBaud(int Baud);

    // Open a device
    char openDevice(const char* Device, const unsigned int Bauds,
        SerialDataBits Databits = SERIAL_DATABITS_8,
        SerialParity Parity = SERIAL_PARITY_NONE,
        SerialStopBits Stopbits = SERIAL_STOPBITS_1);

    // Close the current device
    void closeDevice();

    //___________________________________________
    // ::: Read/Write operation on characters :::

    // Write a char
    char writeChar(char);

    // Read a char (with timeout)
    char readChar(char* pByte, const unsigned int timeOut_ms = 0);

    //________________________________________
    // ::: Read/Write operation on strings :::

    // Write a string
    char writeString(const char* String);

    // Read a string (with timeout)
    int readString(char* receivedString,
        char finalChar,
        unsigned int maxNbBytes,
        const unsigned int timeOut_ms = 0);

    // _____________________________________
    // ::: Read/Write operation on bytes :::

    // Write an array of bytes
    char writeBytes(const void* Buffer, const unsigned int NbBytes);

    // Read an array of byte (with timeout)
    int readBytes(void* buffer,
        unsigned int maxNbBytes,
        const unsigned int timeOut_ms = 0,
        unsigned int sleepDuration_us = 100);

    // _________________________
    // ::: Special operation :::

    // Empty the received buffer
    char flushReceiver();

    // Return the number of bytes in the received buffer
    int available();

    // _________________________
    // ::: Access to IO bits :::

    // Set DTR status (Data Terminal Ready, pin 4)
    bool DTR(bool status);
    bool setDTR();
    bool clearDTR();

    // Set RTS status (Request To Send, pin 7)
    bool RTS(bool status);
    bool setRTS();
    bool clearRTS();

    // Get RI status (Ring Indicator, pin 9)
    bool isRI();

    // Get DCD status (Data Carrier Detect, pin 1)
    bool isDCD();

    // Get CTS status (Clear To Send, pin 8)
    bool isCTS();

    // Get DSR status (Data Set Ready, pin 6)
    bool isDSR();

    // Get RTS status (Request To Send, pin 7)
    bool isRTS();

    // Get DTR status (Data Terminal Ready, pin 4)
    bool isDTR();

private:
    // Read a string (no timeout)
    int readStringNoTimeOut(char* String, char FinalChar, unsigned int MaxNbBytes);

    // Current DTR and RTS state (not reliably readable on Windows)
    bool currentStateRTS = true;
    bool currentStateDTR = true;

#if defined(_WIN32) || defined(_WIN64)
    // Handle on serial device
    HANDLE hSerial = INVALID_HANDLE_VALUE;
    // For setting serial port timeouts
    COMMTIMEOUTS timeouts{};
#else
    int fd = -1;
#endif
};


/*!  \class     timeOut
     \brief     This class can manage a timer which is used as a timeout.

    Platform notes:
    - Windows: uses QueryPerformanceCounter/Frequency (monotonic).
    - Unix: uses gettimeofday (kept for compatibility with original).
*/
class timeOut {
public:
    timeOut();

    // Init the timer
    void initTimer();

    // Return the elapsed time since initialization (milliseconds)
    unsigned long int elapsedTime_ms();

private:
#if defined(_WIN32) || defined(_WIN64)
    // QPC ticks at init, and frequency
    long long startTicks_ = 0;
    long long freq_ = 0;
#else
    // Used to store the previous time (for computing timeout)
    struct timeval previousTime;
#endif
};

#endif // SERIALIB_H
