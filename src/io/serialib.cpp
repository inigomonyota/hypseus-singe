/*!
 \file    serialib.cpp
 \brief   Source file of the class serialib. This class is used for communication over a serial device.
 \author  Philippe Lucidarme (University of Angers)
 \version 2.0
 \date    december the 27th of 2019

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

This is a licence-free software, it can be used by anyone who try to build a better world.

-------------------------------------------------------------------------------
Windows note (MSVC):
- <sys/time.h>, timeval, gettimeofday, usleep, nanosleep are POSIX-only.
- For timeouts, this file uses QueryPerformanceCounter (monotonic, high-res).
-------------------------------------------------------------------------------
*/

#include "serialib.h"

#include <string.h>   // strlen
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>

// If serialib.h does not already define this:
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#else

// POSIX / Unix-like
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <strings.h>   // bzero (legacy) / strcasecmp, etc.

#endif


//_____________________________________
// ::: Constructors and destructors :::

/*!
    \brief      Constructor of the class serialib.
*/
serialib::serialib() {
#if defined(_WIN32) || defined(_WIN64)
    // Default cached states for RTS/DTR on Windows.
    // Windows APIs often report these states inconsistently; we track what we set.
    currentStateRTS = true;
    currentStateDTR = true;
#endif
}


/*!
    \brief      Destructor of the class serialib. It close the connection
*/
serialib::~serialib() {
    closeDevice();
}



//_________________________________________
// ::: Configuration and initialization :::

/*!
     \brief Open the serial port
     \param Device : Port name (COM1, COM2, ... for Windows ) or (/dev/ttyS0, /dev/ttyACM0, /dev/ttyUSB0 ... for linux)
     \param Bauds : Baud rate of the serial port.
     \param Databits : Number of data bits in one UART transmission.
     \param Parity: Parity type
     \param Stopbit: Number of stop bits
     \return 1 success
     \return -1 device not found (Windows)
     \return -2 error while opening the device
     \return -3 error while getting port parameters
     \return -4 Speed (Bauds) not recognized
     \return -5 error while writing port parameters
     \return -6 error while writing timeout parameters
     \return -7 Databits not recognized
     \return -8 Stopbits not recognized
     \return -9 Parity not recognized
*/
char serialib::openDevice(const char* Device, const unsigned int Bauds,
    SerialDataBits Databits,
    SerialParity Parity,
    SerialStopBits Stopbits) {
#if defined(_WIN32) || defined(_WIN64)

    // Open serial port (Device is typically "COM3" or "\\\\.\\COM10" for COM10+)
    hSerial = CreateFileA(Device, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if (hSerial == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
            return -1; // Device not found
        return -2;     // Error while opening
    }

    // Structure for the port parameters
    DCB dcbSerialParams;
    ZeroMemory(&dcbSerialParams, sizeof(dcbSerialParams));
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    // Get the port parameters
    if (!GetCommState(hSerial, &dcbSerialParams))
        return -3;

    // Set the speed (Bauds)
    switch (Bauds)
    {
        case 110:    dcbSerialParams.BaudRate = CBR_110;    break;
        case 300:    dcbSerialParams.BaudRate = CBR_300;    break;
        case 600:    dcbSerialParams.BaudRate = CBR_600;    break;
        case 1200:   dcbSerialParams.BaudRate = CBR_1200;   break;
        case 2400:   dcbSerialParams.BaudRate = CBR_2400;   break;
        case 4800:   dcbSerialParams.BaudRate = CBR_4800;   break;
        case 9600:   dcbSerialParams.BaudRate = CBR_9600;   break;
        case 14400:  dcbSerialParams.BaudRate = CBR_14400;  break;
        case 19200:  dcbSerialParams.BaudRate = CBR_19200;  break;
        case 38400:  dcbSerialParams.BaudRate = CBR_38400;  break;
        case 56000:  dcbSerialParams.BaudRate = CBR_56000;  break;
        case 57600:  dcbSerialParams.BaudRate = CBR_57600;  break;
        case 115200: dcbSerialParams.BaudRate = CBR_115200; break;
        case 128000: dcbSerialParams.BaudRate = CBR_128000; break;
        case 256000: dcbSerialParams.BaudRate = CBR_256000; break;
        default: return -4;
    }

    // Select data size
    BYTE bytesize = 0;
    switch (Databits)
    {
        case SERIAL_DATABITS_5:  bytesize = 5;  break;
        case SERIAL_DATABITS_6:  bytesize = 6;  break;
        case SERIAL_DATABITS_7:  bytesize = 7;  break;
        case SERIAL_DATABITS_8:  bytesize = 8;  break;
        case SERIAL_DATABITS_16: bytesize = 16; break;
        default: return -7;
    }

    // Select stop bits
    BYTE stopBits = 0;
    switch (Stopbits)
    {
        case SERIAL_STOPBITS_1:   stopBits = ONESTOPBIT;   break;
        case SERIAL_STOPBITS_1_5: stopBits = ONE5STOPBITS; break;
        case SERIAL_STOPBITS_2:   stopBits = TWOSTOPBITS;  break;
        default: return -8;
    }

    // Select parity
    BYTE parity = 0;
    switch (Parity)
    {
        case SERIAL_PARITY_NONE:  parity = NOPARITY;   break;
        case SERIAL_PARITY_EVEN:  parity = EVENPARITY; break;
        case SERIAL_PARITY_ODD:   parity = ODDPARITY;  break;
        case SERIAL_PARITY_MARK:  parity = MARKPARITY; break;
        case SERIAL_PARITY_SPACE: parity = SPACEPARITY; break;
        default: return -9;
    }

    // Configure line settings
    dcbSerialParams.ByteSize = bytesize;
    dcbSerialParams.StopBits = stopBits;
    dcbSerialParams.Parity = parity;

    // Write the parameters
    if (!SetCommState(hSerial, &dcbSerialParams))
        return -5;

    // Set TimeOut parameters.
    // Note: library uses SetCommTimeouts per read() call for per-operation timeouts.
    timeouts.ReadIntervalTimeout = 0;
    timeouts.ReadTotalTimeoutConstant = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = MAXDWORD;
    timeouts.WriteTotalTimeoutMultiplier = 0;

    if (!SetCommTimeouts(hSerial, &timeouts))
        return -6;

    return 1;

#else // Unix-like

    struct termios options;

    // Open device
    fd = open(Device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) return -2;

    // Non-blocking mode
    fcntl(fd, F_SETFL, FNDELAY);

    // Get current options
    tcgetattr(fd, &options);
    bzero(&options, sizeof(options));

    // Prepare speed (Bauds)
    speed_t Speed;
    switch (Bauds)
    {
        case 110:    Speed = B110;    break;
        case 300:    Speed = B300;    break;
        case 600:    Speed = B600;    break;
        case 1200:   Speed = B1200;   break;
        case 2400:   Speed = B2400;   break;
        case 4800:   Speed = B4800;   break;
        case 9600:   Speed = B9600;   break;
        case 19200:  Speed = B19200;  break;
        case 38400:  Speed = B38400;  break;
        case 57600:  Speed = B57600;  break;
        case 115200: Speed = B115200; break;
        default: return -4;
    }

    int databits_flag = 0;
    switch (Databits)
    {
        case SERIAL_DATABITS_5: databits_flag = CS5; break;
        case SERIAL_DATABITS_6: databits_flag = CS6; break;
        case SERIAL_DATABITS_7: databits_flag = CS7; break;
        case SERIAL_DATABITS_8: databits_flag = CS8; break;
        default: return -7; // 16-bit and others not supported on Unix
    }

    int stopbits_flag = 0;
    switch (Stopbits)
    {
        case SERIAL_STOPBITS_1: stopbits_flag = 0;      break;
        case SERIAL_STOPBITS_2: stopbits_flag = CSTOPB; break;
        default: return -8; // 1.5 not supported on Unix
    }

    int parity_flag = 0;
    switch (Parity)
    {
        case SERIAL_PARITY_NONE: parity_flag = 0;                break;
        case SERIAL_PARITY_EVEN: parity_flag = PARENB;           break;
        case SERIAL_PARITY_ODD:  parity_flag = (PARENB | PARODD);  break;
        default: return -9; // mark/space not supported on Unix
    }

    cfsetispeed(&options, Speed);
    cfsetospeed(&options, Speed);

    options.c_cflag |= (CLOCAL | CREAD | databits_flag | parity_flag | stopbits_flag);
    options.c_iflag |= (IGNPAR | IGNBRK);

    options.c_cc[VTIME] = 0;
    options.c_cc[VMIN] = 0;

    tcsetattr(fd, TCSANOW, &options);
    return 1;

#endif
}


/*!
     \brief Close the connection with the current device
*/
void serialib::closeDevice() {
#if defined(_WIN32) || defined(_WIN64)
    if (hSerial && hSerial != INVALID_HANDLE_VALUE)
        CloseHandle(hSerial);
    hSerial = INVALID_HANDLE_VALUE;
#else
    if (fd >= 0)
        close(fd);
    fd = -1;
#endif
}



//___________________________________________
// ::: Read/Write operation on characters :::

/*!
     \brief Write a char on the current serial port
     \param Byte : char to send on the port
     \return 1 success
     \return -1 error while writting data
*/
char serialib::writeChar(const char Byte) {
#if defined(_WIN32) || defined(_WIN64)
    DWORD dwBytesWritten = 0;
    if (!WriteFile(hSerial, &Byte, 1, &dwBytesWritten, NULL))
        return -1;
    return 1;
#else
    if (write(fd, &Byte, 1) != 1)
        return -1;
    return 1;
#endif
}



//________________________________________
// ::: Read/Write operation on strings :::

/*!
     \brief     Write a string on the current serial port
     \param     receivedString : string to send on the port (must be terminated by '\0')
     \return     1 success
     \return    -1 error while writting data
*/
char serialib::writeString(const char* receivedString) {
#if defined(_WIN32) || defined(_WIN64)
    DWORD dwBytesWritten = 0;
    if (!WriteFile(hSerial, receivedString, (DWORD)strlen(receivedString), &dwBytesWritten, NULL))
        return -1;
    return 1;
#else
    int Lenght = (int)strlen(receivedString);
    if (write(fd, receivedString, Lenght) != Lenght)
        return -1;
    return 1;
#endif
}



// _____________________________________
// ::: Read/Write operation on bytes :::

/*!
     \brief Write an array of data on the current serial port
     \param Buffer : array of bytes to send on the port
     \param NbBytes : number of byte to send
     \return 1 success
     \return -1 error while writting data
*/
char serialib::writeBytes(const void* Buffer, const unsigned int NbBytes) {
#if defined(_WIN32) || defined(_WIN64)
    DWORD dwBytesWritten = 0;
    if (!WriteFile(hSerial, Buffer, (DWORD)NbBytes, &dwBytesWritten, NULL))
        return -1;
    return 1;
#else
    if (write(fd, Buffer, NbBytes) != (ssize_t)NbBytes)
        return -1;
    return 1;
#endif
}



/*!
     \brief Wait for a byte from the serial device and return the data read
     \param pByte : data read on the serial device
     \param timeOut_ms : delay of timeout before giving up the reading
            If set to zero, timeout is disable (Optional)
     \return 1 success
     \return 0 Timeout reached
     \return -1 error while setting the Timeout
     \return -2 error while reading the byte
*/
char serialib::readChar(char* pByte, unsigned int timeOut_ms) {
#if defined(_WIN32) || defined(_WIN64)
    DWORD dwBytesRead = 0;

    // Per-call timeout
    timeouts.ReadTotalTimeoutConstant = (DWORD)timeOut_ms;

    if (!SetCommTimeouts(hSerial, &timeouts)) return -1;
    if (!ReadFile(hSerial, pByte, 1, &dwBytesRead, NULL)) return -2;

    if (dwBytesRead == 0) return 0;
    return 1;
#else
    timeOut timer;
    timer.initTimer();

    while (timer.elapsedTime_ms() < timeOut_ms || timeOut_ms == 0)
    {
        switch (read(fd, pByte, 1))
        {
            case 1:  return 1;
            case -1: return -2;
            default: break;
        }
    }
    return 0;
#endif
}



/*!
     \brief Read a string from the serial device (without TimeOut)
     \param receivedString : string read on the serial device
     \param FinalChar : final char of the string
     \param MaxNbBytes : maximum allowed number of bytes read
     \return >0 success, return the number of bytes read
     \return -1 error while setting the Timeout
     \return -2 error while reading the byte
     \return -3 MaxNbBytes is reached
*/
int serialib::readStringNoTimeOut(char* receivedString, char finalChar, unsigned int maxNbBytes) {
    unsigned int NbBytes = 0;
    char charRead;

    while (NbBytes < maxNbBytes)
    {
        charRead = readChar(&receivedString[NbBytes]);

        if (charRead == 1)
        {
            if (receivedString[NbBytes] == finalChar)
            {
                receivedString[++NbBytes] = 0;
                return (int)NbBytes;
            }
            NbBytes++;
        }

        if (charRead < 0) return (int)charRead;
    }
    return -3;
}


/*!
     \brief Read a string from the serial device (with timeout)
*/
int serialib::readString(char* receivedString, char finalChar, unsigned int maxNbBytes, unsigned int timeOut_ms) {
    if (timeOut_ms == 0) return readStringNoTimeOut(receivedString, finalChar, maxNbBytes);

    unsigned int nbBytes = 0;
    char charRead;
    timeOut timer;
    long int timeOutParam;

    timer.initTimer();

    while (nbBytes < maxNbBytes)
    {
        timeOutParam = (long int)timeOut_ms - (long int)timer.elapsedTime_ms();

        if (timeOutParam > 0)
        {
            charRead = readChar(&receivedString[nbBytes], (unsigned int)timeOutParam);

            if (charRead == 1)
            {
                if (receivedString[nbBytes] == finalChar)
                {
                    receivedString[++nbBytes] = 0;
                    return (int)nbBytes;
                }
                nbBytes++;
            }

            if (charRead < 0) return (int)charRead;
        }

        if (timer.elapsedTime_ms() > timeOut_ms)
        {
            receivedString[nbBytes] = 0;
            return 0;
        }
    }

    return -3;
}


/*!
     \brief Read an array of bytes from the serial device (with timeout)
*/
int serialib::readBytes(void* buffer, unsigned int maxNbBytes, unsigned int timeOut_ms, unsigned int sleepDuration_us) {
#if defined(_WIN32) || defined(_WIN64)
    UNUSED(sleepDuration_us);

    DWORD dwBytesRead = 0;

    timeouts.ReadTotalTimeoutConstant = (DWORD)timeOut_ms;

    if (!SetCommTimeouts(hSerial, &timeouts)) return -1;
    if (!ReadFile(hSerial, buffer, (DWORD)maxNbBytes, &dwBytesRead, NULL)) return -2;

    return (int)dwBytesRead;

#else
    timeOut timer;
    timer.initTimer();
    unsigned int NbByteRead = 0;

    while (timer.elapsedTime_ms() < timeOut_ms || timeOut_ms == 0)
    {
        unsigned char* Ptr = (unsigned char*)buffer + NbByteRead;
        int Ret = (int)read(fd, (void*)Ptr, maxNbBytes - NbByteRead);

        if (Ret == -1) return -2;

        if (Ret > 0)
        {
            NbByteRead += (unsigned int)Ret;
            if (NbByteRead >= maxNbBytes)
                return (int)NbByteRead;
        }

        usleep(sleepDuration_us);
    }

    return (int)NbByteRead;
#endif
}



// _________________________
// ::: Special operation :::

/*!
    \brief Empty receiver buffer
*/
char serialib::flushReceiver() {
#if defined(_WIN32) || defined(_WIN64)
    return (char)PurgeComm(hSerial, PURGE_RXCLEAR);
#else
    tcflush(fd, TCIFLUSH);
    return true;
#endif
}


/*!
    \brief  Return the number of bytes in the received buffer
*/
int serialib::available() {
#if defined(_WIN32) || defined(_WIN64)
    DWORD commErrors = 0;
    COMSTAT commStatus;
    ClearCommError(hSerial, &commErrors, &commStatus);
    return (int)commStatus.cbInQue;
#else
    int nBytes = 0;
    ioctl(fd, FIONREAD, &nBytes);
    return nBytes;
#endif
}



// __________________
// ::: I/O Access :::

bool serialib::DTR(bool status) {
    return status ? this->setDTR() : this->clearDTR();
}

bool serialib::setDTR() {
#if defined(_WIN32) || defined(_WIN64)
    currentStateDTR = true;
    return EscapeCommFunction(hSerial, SETDTR) != 0;
#else
    int status_DTR = 0;
    ioctl(fd, TIOCMGET, &status_DTR);
    status_DTR |= TIOCM_DTR;
    ioctl(fd, TIOCMSET, &status_DTR);
    return true;
#endif
}

bool serialib::clearDTR() {
#if defined(_WIN32) || defined(_WIN64)
    currentStateDTR = false;
    return EscapeCommFunction(hSerial, CLRDTR) != 0;
#else
    int status_DTR = 0;
    ioctl(fd, TIOCMGET, &status_DTR);
    status_DTR &= ~TIOCM_DTR;
    ioctl(fd, TIOCMSET, &status_DTR);
    return true;
#endif
}

bool serialib::RTS(bool status) {
    return status ? this->setRTS() : this->clearRTS();
}

bool serialib::setRTS() {
#if defined(_WIN32) || defined(_WIN64)
    currentStateRTS = true;
    return EscapeCommFunction(hSerial, SETRTS) != 0;
#else
    int status_RTS = 0;
    ioctl(fd, TIOCMGET, &status_RTS);
    status_RTS |= TIOCM_RTS;
    ioctl(fd, TIOCMSET, &status_RTS);
    return true;
#endif
}

bool serialib::clearRTS() {
#if defined(_WIN32) || defined(_WIN64)
    currentStateRTS = false;
    return EscapeCommFunction(hSerial, CLRRTS) != 0;
#else
    int status_RTS = 0;
    ioctl(fd, TIOCMGET, &status_RTS);
    status_RTS &= ~TIOCM_RTS;
    ioctl(fd, TIOCMSET, &status_RTS);
    return true;
#endif
}

bool serialib::isCTS() {
#if defined(_WIN32) || defined(_WIN64)
    DWORD modemStat = 0;
    GetCommModemStatus(hSerial, &modemStat);
    return (modemStat & MS_CTS_ON) != 0;
#else
    int status = 0;
    ioctl(fd, TIOCMGET, &status);
    return (status & TIOCM_CTS) != 0;
#endif
}

bool serialib::isDSR() {
#if defined(_WIN32) || defined(_WIN64)
    DWORD modemStat = 0;
    GetCommModemStatus(hSerial, &modemStat);
    return (modemStat & MS_DSR_ON) != 0;
#else
    int status = 0;
    ioctl(fd, TIOCMGET, &status);
    return (status & TIOCM_DSR) != 0;
#endif
}

bool serialib::isDCD() {
#if defined(_WIN32) || defined(_WIN64)
    DWORD modemStat = 0;
    GetCommModemStatus(hSerial, &modemStat);
    return (modemStat & MS_RLSD_ON) != 0;
#else
    int status = 0;
    ioctl(fd, TIOCMGET, &status);
    return (status & TIOCM_CAR) != 0;
#endif
}

bool serialib::isRI() {
#if defined(_WIN32) || defined(_WIN64)
    DWORD modemStat = 0;
    GetCommModemStatus(hSerial, &modemStat);
    return (modemStat & MS_RING_ON) != 0;
#else
    int status = 0;
    ioctl(fd, TIOCMGET, &status);
    return (status & TIOCM_RNG) != 0;
#endif
}

bool serialib::isDTR() {
#if defined(_WIN32) || defined(_WIN64)
    return currentStateDTR;
#else
    int status = 0;
    ioctl(fd, TIOCMGET, &status);
    return (status & TIOCM_DTR) != 0;
#endif
}

bool serialib::isRTS() {
#if defined(_WIN32) || defined(_WIN64)
    return currentStateRTS;
#else
    int status = 0;
    ioctl(fd, TIOCMGET, &status);
    return (status & TIOCM_RTS) != 0;
#endif
}

bool serialib::SupportedBaud(int Baud) {
    switch (Baud)
    {
        case 110:
        case 300:
        case 600:
        case 1200:
        case 2400:
        case 4800:
        case 9600:
#if defined(_WIN32) || defined(_WIN64)
        case 14400:
        case 56000:
#endif
        case 19200:
        case 38400:
        case 57600:
        case 115200:
        return true;
        default:
        return false;
    }
}



// ******************************************
//  Class timeOut
// ******************************************

timeOut::timeOut() {}

/*!
    \brief Initialise the timer.
    Windows: QueryPerformanceCounter (monotonic).
    Unix: gettimeofday (wall clock; kept to preserve original behavior).
*/
void timeOut::initTimer() {
#if defined(_WIN32) || defined(_WIN64)
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    freq_ = (long long)f.QuadPart;
    startTicks_ = (long long)c.QuadPart;
#else
    gettimeofday(&previousTime, nullptr);
#endif
}

/*!
    \brief Returns elapsed milliseconds since initTimer().
*/
unsigned long int timeOut::elapsedTime_ms() {
#if defined(_WIN32) || defined(_WIN64)
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);

    const long long ticks = (long long)c.QuadPart - startTicks_;
    // Convert to ms: ticks * 1000 / freq
    return (unsigned long int)((ticks * 1000LL) / freq_);
#else
    struct timeval CurrentTime;
    int sec, usec;

    gettimeofday(&CurrentTime, nullptr);

    sec = (int)(CurrentTime.tv_sec - previousTime.tv_sec);
    usec = (int)(CurrentTime.tv_usec - previousTime.tv_usec);

    if (usec < 0)
    {
        usec += 1000000;
        sec--;
    }

    return (unsigned long int)(sec * 1000 + usec / 1000);
#endif
}
