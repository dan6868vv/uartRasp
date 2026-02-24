#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include <cmath>
#include <errno.h>
#include <wiringSerial.h>
#include <wiringPi.h>
#include <SFML/Graphics.hpp>

// Класс для накопления строки из UART
class UARTLineReader {
private:
    std::string currentLine;
    int fd;
    
public:
    UARTLineReader(int serialFd) : fd(serialFd) {}
    
    // Проверяет, есть ли полная строка (до \n)
    bool readLine(std::string& line) {
        while (serialDataAvail(fd) > 0) {
            char received = serialGetchar(fd);
            
            if (received == '\n') {
                line = currentLine;
                currentLine.clear();
                return true;
            }
            else if (received != '\r') {
                currentLine += received;
            }
        }
        return false;
    }
    
    // Читает и пытается преобразовать в число
    bool readFloat(float& value) {
        std::string line;
        if (readLine(line)) {
            try {
                value = std::stof(line);
                // Нормализуем угол в диапазон 0-360
                value = fmod(value, 360.0f);
                if (value < 0) value += 360.0f;
                return true;
            } catch (const std::exception& e) {
                std::cout << "Получено не число: " << line << std::endl;
            }
        }
        return false;
    }
};

// Класс для отрисовки стрелочного индикатора с PNG
class PNGauge {
private:
    sf::RenderWindow window;
    sf::Texture backgroundTexture;
    sf::Texture needleTexture;
    sf::Sprite backgroundSprite;
    sf::Sprite needleSprite;
    
    sf::Font font;
    sf::Text angleText;
    sf::Text statusText;
    sf::Text debugText;
    
    float currentAngle;
    float targetAngle;
    bool portOpen;
    std::string portName;
    bool texturesLoaded;
    
    // Для отладки - показываем, загрузились ли текстуры
    std::string lastError;
    
public:
    PNGauge(const std::string& port) : 
        window(sf::VideoMode(800, 600), "UART Gauge - Стрелочный индикатор"),
        currentAngle(0.0f),
        targetAngle(0.0f),
        portOpen(true),
        portName(port),
        texturesLoaded(false) {
        
        window.setFramerateLimit(60);
        
        // Загружаем фоновое изображение
        if (!backgroundTexture.loadFromFile("background.png")) {
            lastError = "Не удалось загрузить background.png";
            std::cerr << lastError << std::endl;
        } else {
            backgroundSprite.setTexture(backgroundTexture);
            
            // Масштабируем фон под размер окна, если нужно
            sf::Vector2u texSize = backgroundTexture.getSize();
            float scaleX = 800.0f / texSize.x;
            float scaleY = 600.0f / texSize.y;
            backgroundSprite.setScale(scaleX, scaleY);
        }
        
        // Загружаем изображение стрелки
        if (!needleTexture.loadFromFile("needle.png")) {
            lastError = "Не удалось загрузить needle.png";
            std::cerr << lastError << std::endl;
        } else {
            needleSprite.setTexture(needleTexture);
            
            // Устанавливаем центр вращения в центр изображения
            sf::Vector2u texSize = needleTexture.getSize();
            needleSprite.setOrigin(texSize.x / 2.0f, texSize.y / 2.0f);
            
            texturesLoaded = true;
        }
        
        // Размещаем стрелку по центру окна
        needleSprite.setPosition(400, 300);
        
        // Загружаем шрифт
        if (font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
            angleText.setFont(font);
            angleText.setCharacterSize(36);
            angleText.setFillColor(sf::Color::White);
            angleText.setStyle(sf::Text::Bold);
            angleText.setOutlineColor(sf::Color::Black);
            angleText.setOutlineThickness(2);
            
            statusText.setFont(font);
            statusText.setCharacterSize(18);
            statusText.setFillColor(sf::Color(150, 255, 150));
            
            debugText.setFont(font);
            debugText.setCharacterSize(16);
            debugText.setFillColor(sf::Color::Yellow);
        }
        
        // Позиционируем текст
        angleText.setPosition(300, 500);
        statusText.setPosition(10, 10);
        debugText.setPosition(10, 560);
    }
    
    bool isOpen() const {
        return window.isOpen();
    }
    
    void setPortStatus(bool open) {
        portOpen = open;
    }
    
    void setTargetAngle(float angle) {
        targetAngle = angle;
    }
    
    void update() {
        // Плавное движение стрелки
        float angleDiff = targetAngle - currentAngle;
        
        // Нормализуем разницу (кратчайший путь)
        if (angleDiff > 180) angleDiff -= 360;
        if (angleDiff < -180) angleDiff += 360;
        
        // Плавное движение (коэффициент 0.1 для сглаживания)
        currentAngle += angleDiff * 0.1f;
        
        // Нормализуем угол
        if (currentAngle < 0) currentAngle += 360;
        if (currentAngle >= 360) currentAngle -= 360;
        
        // Поворачиваем стрелку
        needleSprite.setRotation(currentAngle);
        
        // Обновляем текстовую информацию
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Угол: %.1f°", currentAngle);
        angleText.setString(buffer);
        
        if (!portOpen) {
            statusText.setString("❌ Порт закрыт!");
            statusText.setFillColor(sf::Color(255, 100, 100));
        } else {
            statusText.setString("✅ Чтение: " + portName);
            statusText.setFillColor(sf::Color(150, 255, 150));
        }
        
        if (!texturesLoaded) {
            debugText.setString("⚠️ " + lastError);
        } else {
            debugText.setString("✓ Текстуры загружены");
        }
    }
    
    void draw() {
        window.clear(sf::Color(20, 20, 30));
        
        // Рисуем фон, если загружен
        if (backgroundTexture.getSize().x > 0) {
            window.draw(backgroundSprite);
        } else {
            // Если фона нет, рисуем серый круг как заглушку
            sf::CircleShape placeholder(200);
            placeholder.setFillColor(sf::Color(50, 50, 50));
            placeholder.setOutlineColor(sf::Color(100, 100, 100));
            placeholder.setOutlineThickness(3);
            placeholder.setOrigin(200, 200);
            placeholder.setPosition(400, 300);
            window.draw(placeholder);
            
            // Рисуем простые метки
            for (int i = 0; i < 12; i++) {
                float angle = i * 30.0f * 3.14159f / 180.0f;
                float radius = 180.0f;
                float x1 = 400 + cos(angle) * radius;
                float y1 = 300 + sin(angle) * radius;
                float x2 = 400 + cos(angle) * (radius + 20);
                float y2 = 300 + sin(angle) * (radius + 20);
                
                sf::Vertex line[] = {
                    sf::Vertex(sf::Vector2f(x1, y1), sf::Color(200, 200, 200)),
                    sf::Vertex(sf::Vector2f(x2, y2), sf::Color(200, 200, 200))
                };
                window.draw(line, 2, sf::Lines);
            }
        }
        
        // Рисуем стрелку, если загружена
        if (needleTexture.getSize().x > 0) {
            window.draw(needleSprite);
        } else {
            // Если стрелки нет, рисуем простую линию как заглушку
            sf::RectangleShape fallbackNeedle(sf::Vector2f(100, 5));
            fallbackNeedle.setFillColor(sf::Color::Red);
            fallbackNeedle.setOrigin(0, 2.5f);
            fallbackNeedle.setPosition(400, 300);
            fallbackNeedle.setRotation(currentAngle);
            window.draw(fallbackNeedle);
            
            sf::CircleShape dot(8);
            dot.setFillColor(sf::Color::White);
            dot.setOrigin(8, 8);
            dot.setPosition(400, 300);
            window.draw(dot);
        }
        
        // Рисуем текст
        window.draw(angleText);
        window.draw(statusText);
        window.draw(debugText);
        
        window.display();
    }
    
    void handleEvents() {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                window.close();
            }
            // Нажатие 'D' для отладки - показать позицию мыши
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::D) {
                sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                std::cout << "Mouse: " << mousePos.x << ", " << mousePos.y << std::endl;
            }
        }
    }
};

int main(int argc, char** argv) {
    // Параметры по умолчанию
    std::string port = "/dev/serial0";
    int baudRate = 9600;
    
    // Можно передать порт и скорость как аргументы командной строки
    if (argc > 1) {
        port = argv[1];
    }
    if (argc > 2) {
        baudRate = std::stoi(argv[2]);
    }
    
    std::cout << "======================================" << std::endl;
    std::cout << "UART Gauge с PNG поддержкой" << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << "Порт: " << port << std::endl;
    std::cout << "Скорость: " << baudRate << std::endl;
    std::cout << "Файлы:" << std::endl;
    std::cout << "  - background.png (фон/циферблат)" << std::endl;
    std::cout << "  - needle.png (стрелка)" << std::endl;
    std::cout << "======================================" << std::endl;
    
    // Инициализация wiringPi
    if (wiringPiSetup() == -1) {
        std::cerr << "❌ Ошибка инициализации wiringPi" << std::endl;
        return 1;
    }
    
    // Открываем последовательный порт
    int fd = serialOpen(port.c_str(), baudRate);
    
    if (fd == -1) {
        std::cerr << "❌ Не удалось открыть порт: " << strerror(errno) << std::endl;
        std::cerr << "⚠️ Продолжаем в демо-режиме (без UART)..." << std::endl;
    } else {
        std::cout << "✅ Порт открыт успешно. Ожидание данных..." << std::endl;
    }
    
    // Создаем графическое окно
    PNGauge gauge(port);
    gauge.setPortStatus(fd != -1);
    
    // Создаем читатель UART (только если порт открыт)
    UARTLineReader* reader = nullptr;
    if (fd != -1) {
        reader = new UARTLineReader(fd);
    }
    
    // Переменная для хранения угла
    float angle = 0.0f;
    float demoAngle = 0.0f;
    unsigned int frameCount = 0;
    
    // Главный цикл приложения
    while (gauge.isOpen()) {
        // Обработка событий окна
        gauge.handleEvents();
        
        // Чтение из UART (если порт открыт)
        if (fd != -1 && reader != nullptr) {
            if (reader->readFloat(angle)) {
                gauge.setTargetAngle(angle);
            }
        } else {
            // Демо-режим: плавно вращаем стрелку
            demoAngle += 0.2f;
            if (demoAngle >= 360) demoAngle -= 360;
            gauge.setTargetAngle(demoAngle);
            
            // Каждые 600 кадров (примерно 10 секунд) напоминаем о демо-режиме
            frameCount++;
            if (frameCount % 600 == 0) {
                std::cout << "Демо-режим: стрелка вращается. Подключите UART для реальных данных." << std::endl;
            }
        }
        
        // Обновляем состояние стрелки
        gauge.update();
        
        // Отрисовываем
        gauge.draw();
        
        // Небольшая задержка для экономии CPU
        delay(5);
    }
    
    // Очистка
    if (reader != nullptr) {
        delete reader;
    }
    if (fd != -1) {
        serialClose(fd);
        std::cout << "Порт закрыт" << std::endl;
    }
    
    return 0;
}