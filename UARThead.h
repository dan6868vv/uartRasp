//
// Created by Пользователь on 17.02.2026.
//
#include <windows.h>
#include <iostream>
#include <io.h>
#ifndef UARTHEAD_H
#define UARTHEAD_H

#endif //UARTHEAD_H

HANDLE hSerialfromHead = CreateFile(
        "\\\\.\\COM7",              // Имя порта (специальный синтаксис)
        GENERIC_WRITE,                // Только запись
        0,                            // Эксклюзивный доступ
        NULL,                         // Без атрибутов безопасности
        OPEN_EXISTING,                 // Открываем существующий порт
        FILE_ATTRIBUTE_NORMAL,        // Обычный файл
        NULL
    );

bool sendDataToComPortChar(HANDLE hSerial, const char* data, DWORD &bytesWritten) {

    if (!WriteFile(hSerial, data, strlen(data), &bytesWritten, NULL)) {
        std::cerr << "Error sent to port" << std::endl;
        return false;
    }
    else {
        if(data != "\r\n") {
            std::cout << "Sent " << bytesWritten << " bytes| text '" << data << "'" << std::endl;
        }
        return true;
    }
    return false;
}

bool sendDataToComPortInt(HANDLE hSerial,int number , DWORD &bytesWritten) {
    std::string str = std::to_string(number);
    const char* data = str.c_str();
    if (!WriteFile(hSerial, data, strlen(data), &bytesWritten, NULL)) {
        std::cerr << "Error sent to port" << std::endl;
        return false;
    }
    else {
        if(data != "\r\n") {
            std::cout << "Sent " << bytesWritten << " bytes| text '" << data << "'" << std::endl;
        }
        return true;
    }
    return false;
}
