# Имя исполняемого файла
TARGET = uart_png

# Исходные файлы
SOURCES = uart_png.cpp

# Компилятор и флаги
CXX = g++
CXXFLAGS = -O2 -Wall
LDFLAGS = -lwiringPi -lsfml-graphics -lsfml-window -lsfml-system

# Правило сборки
$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

# Очистка
clean:
	rm -f $(TARGET)

# Установка (опционально)
install:
	sudo cp $(TARGET) /usr/local/bin/

