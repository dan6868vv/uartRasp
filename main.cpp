
#include <charconv>
#include <fcntl.h>
#include "UARThead.h"
int main() {


    // 1. Открываем COM порт

    HANDLE hSerial = hSerialfromHead;

    if (hSerial == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: failed to open COM7 (code: " << GetLastError() << ")" << std::endl;
        return 1;
    }

    // 2. Настраиваем параметры порта

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Error getting port parameters" << std::endl;
        CloseHandle(hSerial);
        return 1;
    }

    // Установка стандартных параметров (9600 бод, 8 бит, 1 стоп-бит, без четности)
    dcbSerialParams.BaudRate = CBR_9600;      // Скорость 9600
    dcbSerialParams.ByteSize = 8;              // 8 бит данных
    dcbSerialParams.StopBits = ONESTOPBIT;     // 1 стоп-бит
    dcbSerialParams.Parity = NOPARITY;         // Без четности

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Error set port settings" << std::endl;
        CloseHandle(hSerial);
        return 1;
    }

    // 3. Отправляем данные
    const char* data = "12,12,45,6\r\n";  // \r\n для перевода строки на приемнике
    DWORD bytesWritten;

    const int N = 100;
    int a[N];
    for (int *d = a, *end = d + N; d < end; ++d)
    {
         *d = rand()%100;
    } 

    for(int i=0;i<N;i++) {

        sendDataToComPortInt(hSerial, a[i], bytesWritten);
        _sleep(30);
        sendDataToComPortChar(hSerial, "\r\n", bytesWritten);
        
    }

    // 4. Закрываем порт
    CloseHandle(hSerial);
    return 0;
}