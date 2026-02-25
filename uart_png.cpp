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
#include <fstream>  // Для чтения конфигурационного файла
#include <map>      // Для хранения пар ключ-значение из конфига

std::vector<std::string> loadConfigPngFiles(const std::string &filename) {
    std::map<std::string, std::string> config;
    std::ifstream file(filename);
    std::vector<std::string> nameFiles;
    if (!file.is_open()) {
        std::cout << "Конфиг файл установки изображений не найден. Бдут использованы needle.png и background.png" <<
                std::endl;
        nameFiles.push_back("needle.png");
        nameFiles.push_back("background.png");
        return nameFiles;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string key;
        std::string equals;
        std::string value;
        if (iss >> key >> equals >> value && equals == "=") {
            config[key] = value;
            std::cout << "Данные из pic_cofig.txt: " << key << " = " << value << std::endl;
        }
    }

    file.close();
    if (config.count("needle_file_name")) {
        nameFiles.push_back(config["needle_file_name"]);
    } else {
        nameFiles.push_back("needle.png");
    }
    if (config.count("background_file_name")) {
        nameFiles.push_back(config["background_file_name"]);
    } else {
        nameFiles.push_back("background.png");
    }
    return nameFiles;
}

// Класс для накопления строки из UART
class UARTLineReader {
private:
    std::string currentLine;
    int fd;

public:
    UARTLineReader(int serialFd) : fd(serialFd) {
    }

    // Проверяет, есть ли полная строка (до \n)
    bool readLine(std::string &line) {
        while (serialDataAvail(fd) > 0) {
            char received = serialGetchar(fd);

            if (received == '\n') {
                line = currentLine;
                currentLine.clear();
                return true;
            } else if (received != '\r') {
                currentLine += received;
            }
        }
        return false;
    }

    // Читает и пытается преобразовать в число
    bool readFloat(float &value) {
        std::string line;
        if (readLine(line)) {
            try {
                value = std::stof(line);
                // Нормализуем угол в диапазон 0-360
                value = fmod(value, 360.0f);
                if (value < 0) value += 360.0f;
                return true;
            } catch (const std::exception &e) {
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

    int windowX;
    int windowY;
    int centerX;
    int centerY;

    bool isAngleText = false;
    // NEW: для конфигурации точки вращения
    float pivotX;
    float pivotY;
    std::string configFile;
    bool useConfig;

    // Для отладки - показываем, загрузились ли текстуры
    std::string lastError;

public:
    PNGauge(const std::string &port, std::string needleFileName,
            std::string backgroundFileName) : window(sf::VideoMode(800, 600), "UART Gauge - Стрелочный индикатор"),
                                              currentAngle(0.0f),
                                              targetAngle(0.0f),
                                              portOpen(true),
                                              portName(port),
                                              texturesLoaded(false) {
        window.setFramerateLimit(60);
        // NEW: инициализация переменных конфигурации
        pivotX = 0;
        pivotY = 0;
        windowX = 800;
        windowY = 600;
        centerX = 400;
        centerY = 300;
        configFile = "needle_config.txt";
        useConfig = false;

        // Загружаем фоновое изображение
        if (!backgroundTexture.loadFromFile(backgroundFileName)) {
            lastError = "Не удалось загрузить " + backgroundFileName;
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
        if (!needleTexture.loadFromFile(needleFileName)) {
            lastError = "Не удалось загрузить " + needleFileName;
            std::cerr << lastError << std::endl;
        } else {
            needleSprite.setTexture(needleTexture);

            // NEW: загружаем конфигурацию и устанавливаем точку вращения
            loadConfig(configFile);
            texturesLoaded = true;
        }


        // Загружаем шрифт
        if (font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
            angleText.setFont(font);
            angleText.setCharacterSize(36);
            angleText.setFillColor(sf::Color::White);
            angleText.setStyle(sf::Text::Bold);
            angleText.setOutlineColor(sf::Color::Black);
            angleText.setOutlineThickness(2);
        }
    }

    bool isOpen() const {
        return window.isOpen();
    }

    void setPortStatus(bool open) {
        portOpen = open;
    }


    // NEW: метод для загрузки конфигурации
    void loadConfig(const std::string &filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cout << "Конфиг файл не найден. Использую центр по умолчанию." << std::endl;
            sf::Vector2u texSize = needleTexture.getSize();
            pivotX = texSize.x / 2.0f;
            pivotY = texSize.y / 2.0f;
            needleSprite.setOrigin(pivotX, pivotY);
            return;
        }

        std::map<std::string, float> config;
        std::string line;

        while (std::getline(file, line)) {
            // Пропускаем комментарии и пустые строки
            if (line.empty() || line[0] == '#') continue;

            std::istringstream iss(line);
            std::string key;
            std::string equals;
            float value;

            if (iss >> key >> equals >> value && equals == "=") {
                config[key] = value;
                std::cout << "Конфиг: " << key << " = " << value << std::endl;
            }
        }

        file.close();

        // Получаем размер текстуры для процентных значений
        sf::Vector2u texSize = needleTexture.getSize();

        // Устанавливаем значения из конфига
        if (config.count("pivot_x_percent")) {
            pivotX = (config["pivot_x_percent"] / 100.0f) * texSize.x;
        } else if (config.count("pivot_x")) {
            pivotX = config["pivot_x"];
        } else {
            pivotX = texSize.x / 2.0f; // центр по умолчанию
        }

        if (config.count("pivot_y_percent")) {
            pivotY = (config["pivot_y_percent"] / 100.0f) * texSize.y;
        } else if (config.count("pivot_y")) {
            pivotY = config["pivot_y"];
        } else {
            pivotY = texSize.y / 2.0f; // центр по умолчанию
        }

        if (config.count("window_x")) {
            windowX = static_cast<int>(config["window_x"]);
        }
        if (config.count("window_y")) {
            windowY = static_cast<int>(config["window_y"]);
        }

        if (config.count("center_x")) {
            centerX = static_cast<int>(config["center_x"]);
        } else {
            centerX = 300;
        }
        if (config.count("center_y")) {
            centerY = static_cast<int>(config["center_y"]);
        } else {
            centerY = 300;
        }

        if (config.count("is_angle_text")) {
            isAngleText = static_cast<int>(config["is_angle_text"]);
        }
        std::cout << "Установлен центр вращения: (" << pivotX << ", " << pivotY << ")" << std::endl;
        needleSprite.setOrigin(pivotX, pivotY);

        // NEW: обновляем размер окна
        window.create(sf::VideoMode(windowX, windowY), "UART Gauge - Стрелочный индикатор");
        window.setFramerateLimit(60);

        // NEW: обновляем масштаб фона под новый размер окна
        if (backgroundTexture.getSize().x > 0) {
            sf::Vector2u texSize = backgroundTexture.getSize();
            float scaleX = static_cast<float>(windowX) / texSize.x;
            float scaleY = static_cast<float>(windowY) / texSize.y;
            backgroundSprite.setScale(scaleX, scaleY);
            std::cout << "Масштаб фона: " << scaleX << "x" << scaleY << std::endl;
        }
        backgroundSprite.setPosition(0, 0);
        needleSprite.setOrigin(pivotX, pivotY);
        // NEW: обновляем позицию стрелки для нового размера окна
        needleSprite.setPosition(centerX, centerY);

        // NEW: обновляем позиции текста
        angleText.setPosition(windowX - 300, windowY - 80);
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
        snprintf(buffer, sizeof(buffer), "Angle: %.1f°", currentAngle);
        angleText.setString(buffer);
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
        if (isAngleText == 1) {
            window.draw(angleText);
        }

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

int main(int argc, char **argv) {
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
    std::vector nameFiles = loadConfigPngFiles("pic_config.txt");
    std::cout << nameFiles.at(0) << std::endl;
    std::cout << nameFiles.at(1) << std::endl;

    PNGauge gauge(port, nameFiles.at(0), nameFiles.at(1));
    gauge.setPortStatus(fd != -1);

    // Создаем читатель UART (только если порт открыт)
    UARTLineReader *reader = nullptr;
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
