#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <chrono>
#include <thread>
#include <vector>
#include <csignal>
#include <iostream>
#include <atomic>

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
atomic<int> current_timer(0);
atomic<bool> running_program(true);

// --- добавено
atomic<bool> lap_mode(false);
atomic<size_t> lap_index(0);

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

void setCursor(int page, int col) {
    sendCommand(0xB0 + page);
    sendCommand(0x00 + (col & 0x0F));
    sendCommand(0x10 + ((col >> 4) & 0x0F));
}

void clearDisplay() {
    for (int page = 0; page < 8; ++page) {
        setCursor(page, 0);
        for (int col = 0; col < 132; ++col) sendData(0x00);
    }
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

void sigintHandler(int) {
    running_program = false;
}

void updateLEDs(int current) {
    digitalWrite(LED_TIMER1, current == 0 ? HIGH : LOW);
    digitalWrite(LED_TIMER2, current == 1 ? HIGH : LOW);
    digitalWrite(LED_TIMER3, current == 2 ? HIGH : LOW);
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

    updateLEDs(current_timer.load());

    atomic<bool> waiting_for_long_press(false);
    steady_clock::time_point press_start_time;

    atomic<bool> last_start(false);
    atomic<bool> last_mode(false);
    // --- добавено
    static bool last_lap = false;

    thread buttonThread([&]() {
        while (running_program.load()) {
            bool start_btn = isPressed(START_BUTTON);
            bool stop_btn = isPressed(STOP_BUTTON);
            bool pause_btn = isPressed(PAUSE_BUTTON);
            bool reset_btn = isPressed(RESET_BUTTON);
            bool lap_btn = isPressed(LAP_BUTTON);
            bool mode_btn = isPressed(MODE_SWITCH_BUTTON);

            int current = current_timer.load();

            if (mode_btn && !last_mode.load()) {
                current = (current + 1) % NUM_TIMERS;
                current_timer = current;
            }
            last_mode = mode_btn;

            if (lap_btn && !last_lap) {
                lap_mode = true;
                lap_index = 0;
            }
            last_lap = lap_btn;

            if (start_btn && !last_start.load()) {
                if (lap_mode.load()) {
                    lap_mode = false;
                } else {
                    Timer& t = timers[current];
                    if (!t.running) {
                        t.running = true;
                        t.paused = false;
                        t.start_time = steady_clock::now();
                    } else if (t.paused) {
                        t.paused = false;
                        t.start_time = steady_clock::now();
                    }
                }
            }

            if (stop_btn) {
                Timer& t = timers[current];
                if (t.running) {
                    t.elapsed += duration_cast<milliseconds>(steady_clock::now() - t.start_time);
                    t.running = false;
                    t.paused = false;
                }
            }

            last_start = start_btn;

            this_thread::sleep_for(milliseconds(20));
        }
    });

    thread ledThread([&]() {
        while (running_program.load()) {
            updateLEDs(current_timer.load());
            this_thread::sleep_for(milliseconds(100));
        }
        digitalWrite(LED_TIMER1, LOW);
        digitalWrite(LED_TIMER2, LOW);
        digitalWrite(LED_TIMER3, LOW);
    });

    int last_minutes = -1, last_seconds = -1, last_centiseconds = -1;
    while (running_program.load()) {
        int current = current_timer.load();
        Timer& t = timers[current];
        milliseconds display_time = t.elapsed;
        if (t.running && !t.paused)
            display_time += duration_cast<milliseconds>(steady_clock::now() - t.start_time);

        if (lap_mode.load() && !t.laps.empty()) {
            milliseconds lap_time = t.laps[lap_index % t.laps.size()];
            int total_ms = lap_time.count();
            int minutes = total_ms / 60000;
            int seconds = (total_ms / 1000) % 60;
            int centiseconds = (total_ms % 1000) / 10;

            drawTimeWithMillis(minutes, seconds, centiseconds);
            lap_index++;
            this_thread::sleep_for(milliseconds(1000));
            continue;
        }

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

        this_thread::sleep_for(milliseconds(50));
    }

    clearDisplay();

    if (buttonThread.joinable()) buttonThread.join();
    if (ledThread.joinable()) ledThread.join();

    return 0;
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

