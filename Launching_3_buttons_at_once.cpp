#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <chrono>
#include <thread>
#include <vector>
#include <csignal>
#include <iostream>

using namespace std;
using namespace std::chrono;

constexpr int SH1106_ADDR = 0x3C;
int fd = -1;

const int START_BUTTON = 18;
const int STOP_BUTTON = 23;
const int PAUSE_BUTTON = 24;
const int RESET_BUTTON = 25;
const int LAP_BUTTON = 5;
const int MODE_SWITCH_BUTTON = 6;

const int LED_TIMER1 = 26;
const int LED_TIMER2 = 17;
const int LED_TIMER3 = 27;

const int NUM_TIMERS = 3;

struct Timer {
    bool running = false;
    bool paused = false;
    steady_clock::time_point start_time;
    milliseconds elapsed{0};
    vector<milliseconds> laps;
};

Timer timers[NUM_TIMERS];
int current_timer = 0;
volatile bool running_program = true;

const uint8_t bigDigits16x8[10][16] = {
    {0x3C,0x00,0x42,0x00,0x81,0x00,0x81,0x00,0x81,0x00,0x81,0x00,0x42,0x00,0x3C,0x00},
    {0x00,0x00,0x84,0x00,0xFE,0x00,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0xC2,0x00,0xA1,0x00,0x91,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x86,0x00,0x00,0x00},
    {0x42,0x00,0x81,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x76,0x00,0x00,0x00},
    {0x30,0x00,0x28,0x00,0x24,0x00,0x22,0x00,0xFE,0x00,0x20,0x00,0x20,0x00,0x00,0x00},
    {0x4F,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x71,0x00,0x00,0x00},
    {0x7E,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x72,0x00,0x00,0x00},
    {0x01,0x00,0xE1,0x00,0x11,0x00,0x09,0x00,0x05,0x00,0x03,0x00,0x01,0x00,0x00,0x00},
    {0x76,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x76,0x00,0x00,0x00},
    {0x06,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x49,0x00,0x3E,0x00,0x00,0x00}
};

void sendCommand(uint8_t cmd) {
    wiringPiI2CWriteReg8(fd, 0x00, cmd);
}

void sendData(uint8_t data) {
    wiringPiI2CWriteReg8(fd, 0x40, data);
}

void initDisplay() {
    sendCommand(0xAE);
    sendCommand(0xD5); sendCommand(0x80);
    sendCommand(0xA8); sendCommand(0x3F);
    sendCommand(0xD3); sendCommand(0x00);
    sendCommand(0x40);
    sendCommand(0xAD); sendCommand(0x8B);
    sendCommand(0xA1);
    sendCommand(0xC8);
    sendCommand(0xDA); sendCommand(0x12);
    sendCommand(0x81); sendCommand(0xCF);
    sendCommand(0xD9); sendCommand(0xF1);
    sendCommand(0xDB); sendCommand(0x40);
    sendCommand(0xA4);
    sendCommand(0xA6);
    sendCommand(0xAF);
}

void setCursor(int page, int col);

void clearDisplay() {
    for (int page = 0; page < 8; ++page) {
        setCursor(page, 0);
        for (int col = 0; col < 132; ++col) sendData(0x00);
    }
}

void setCursor(int page, int col) {
    sendCommand(0xB0 + page);
    sendCommand(0x00 + (col & 0x0F));
    sendCommand(0x10 + ((col >> 4) & 0x0F));
}

void drawBigDigit16x8(int page, int col, int digit) {
    if (digit < 0 || digit > 9) return;
    setCursor(page, col);
    for (int i = 0; i < 8; ++i) sendData(bigDigits16x8[digit][i * 2]);
    setCursor(page + 1, col);
    for (int i = 0; i < 8; ++i) sendData(bigDigits16x8[digit][i * 2 + 1]);
}

void drawColon(int page, int col) {
    setCursor(page, col);
    sendData(0x18);
    setCursor(page + 1, col);
    sendData(0x18);
}

bool isPressed(int pin) {
    return digitalRead(pin) == LOW;
}

void updateLEDs() {
    digitalWrite(LED_TIMER1, current_timer == 0 ? HIGH : LOW);
    digitalWrite(LED_TIMER2, current_timer == 1 ? HIGH : LOW);
    digitalWrite(LED_TIMER3, current_timer == 2 ? HIGH : LOW);
}

void drawTimeWithMillis(int minutes, int seconds, int centiseconds) {
    drawBigDigit16x8(2, 20, minutes / 10);
    drawBigDigit16x8(2, 28, minutes % 10);
    drawColon(2, 36);
    drawBigDigit16x8(2, 42, seconds / 10);
    drawBigDigit16x8(2, 50, seconds % 10);
    drawColon(2, 58);
    drawBigDigit16x8(2, 64, centiseconds / 10);
    drawBigDigit16x8(2, 72, centiseconds % 10);
}

void sigintHandler(int) {
    running_program = false;
}

int main() {
    signal(SIGINT, sigintHandler);
    wiringPiSetupGpio();

    pinMode(START_BUTTON, INPUT); pullUpDnControl(START_BUTTON, PUD_UP);
    pinMode(STOP_BUTTON, INPUT); pullUpDnControl(STOP_BUTTON, PUD_UP);
    pinMode(PAUSE_BUTTON, INPUT); pullUpDnControl(PAUSE_BUTTON, PUD_UP);
    pinMode(RESET_BUTTON, INPUT); pullUpDnControl(RESET_BUTTON, PUD_UP);
    pinMode(LAP_BUTTON, INPUT); pullUpDnControl(LAP_BUTTON, PUD_UP);
    pinMode(MODE_SWITCH_BUTTON, INPUT); pullUpDnControl(MODE_SWITCH_BUTTON, PUD_UP);

    pinMode(LED_TIMER1, OUTPUT);
    pinMode(LED_TIMER2, OUTPUT);
    pinMode(LED_TIMER3, OUTPUT);

    fd = wiringPiI2CSetup(SH1106_ADDR);
    if (fd == -1) {
        cerr << "Неуспешна инициализация на I2C дисплея\n";
        return 1;
    }

    initDisplay();
    clearDisplay();
    updateLEDs();

    bool last_start = false, last_mode = false;
    static bool last_stop = false;   // <-- добавено static тук
    int last_minutes = -1, last_seconds = -1, last_centiseconds = -1;

    bool waiting_for_long_press = false;
    steady_clock::time_point press_start_time;

    while (running_program) {
        bool start_btn = isPressed(START_BUTTON);
        bool stop_btn = isPressed(STOP_BUTTON);
        bool mode_btn = isPressed(MODE_SWITCH_BUTTON);

        if (mode_btn && !last_mode) {
            current_timer = (current_timer + 1) % NUM_TIMERS;
            updateLEDs();
        }

        // Продължително натискане за старт на всички
        if (start_btn && !waiting_for_long_press) {
            waiting_for_long_press = true;
            press_start_time = steady_clock::now();
        } else if (!start_btn && waiting_for_long_press) {
            waiting_for_long_press = false;
        }

        if (waiting_for_long_press &&
            duration_cast<milliseconds>(steady_clock::now() - press_start_time).count() >= 1000) {
            for (int i = 0; i < NUM_TIMERS; ++i) {
                timers[i].running = true;
                timers[i].paused = false;
                timers[i].start_time = steady_clock::now();
            }
            digitalWrite(LED_TIMER1, HIGH);
            digitalWrite(LED_TIMER2, HIGH);
            digitalWrite(LED_TIMER3, HIGH);
            waiting_for_long_press = false;
        }

        Timer& t = timers[current_timer];

        // Обработка на стоп бутона
        if (stop_btn && !last_stop) {
            if (t.running) {
                if (!t.paused) {
                    t.elapsed += duration_cast<milliseconds>(steady_clock::now() - t.start_time);
                }
                t.running = false;
                t.paused = false;
                cout << "Хронометър " << (current_timer + 1) << " е спрян.\n";
            }
        }
        last_stop = stop_btn;

        milliseconds display_time = t.elapsed;
        if (t.running && !t.paused)
            display_time += duration_cast<milliseconds>(steady_clock::now() - t.start_time);

        int total_ms = display_time.count();
        int minutes = total_ms / 60000;
        int seconds = (total_ms / 1000) % 60;
        int centiseconds = (total_ms % 1000) / 10;

        if (minutes != last_minutes || seconds != last_seconds || centiseconds != last_centiseconds) {
            drawTimeWithMillis(minutes, seconds, centiseconds);
            last_minutes = minutes;
            last_seconds = seconds;
            last_centiseconds = centiseconds;
        }

        last_start = start_btn;
        last_mode = mode_btn;
        this_thread::sleep_for(milliseconds(50));
    }

    clearDisplay();
    digitalWrite(LED_TIMER1, LOW);
    digitalWrite(LED_TIMER2, LOW);
    digitalWrite(LED_TIMER3, LOW);
    return 0;
}
