#pragma once

#include <optional>

#ifdef _WIN32

#include <conio.h>

namespace snakecraft {

class Terminal {
public:
    Terminal() = default;
    ~Terminal() = default;

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    [[nodiscard]] std::optional<char> readKey() const
    {
        if (!_kbhit()) {
            return std::nullopt;
        }

        const int key = _getch();
        if (key == 224 || key == 0) {
            const int arrow = _getch();
            switch (arrow) {
            case 72:
                return 'w';
            case 80:
                return 's';
            case 75:
                return 'a';
            case 77:
                return 'd';
            default:
                return std::nullopt;
            }
        }

        return static_cast<char>(key);
    }
};

} // namespace snakecraft

#else

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace snakecraft {

class Terminal {
public:
    Terminal()
    {
        enableRawMode();
    }

    ~Terminal()
    {
        disableRawMode();
    }

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    [[nodiscard]] std::optional<char> readKey() const
    {
        unsigned char key = 0;
        const ssize_t readCount = ::read(STDIN_FILENO, &key, 1);

        if (readCount <= 0) {
            return std::nullopt;
        }

        if (key != '\x1b') {
            return static_cast<char>(key);
        }

        unsigned char sequence[2] {};
        if (::read(STDIN_FILENO, &sequence[0], 1) <= 0) {
            return std::nullopt;
        }

        if (::read(STDIN_FILENO, &sequence[1], 1) <= 0) {
            return std::nullopt;
        }

        if (sequence[0] != '[') {
            return std::nullopt;
        }

        switch (sequence[1]) {
        case 'A':
            return 'w';
        case 'B':
            return 's';
        case 'C':
            return 'd';
        case 'D':
            return 'a';
        default:
            return std::nullopt;
        }
    }

private:
    void enableRawMode()
    {
        if (::tcgetattr(STDIN_FILENO, &originalTermios_) == -1) {
            return;
        }

        rawEnabled_ = true;

        termios raw = originalTermios_;
        raw.c_lflag &= static_cast<unsigned long>(~(ECHO | ICANON));
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

        originalFlags_ = ::fcntl(STDIN_FILENO, F_GETFL, 0);
        if (originalFlags_ != -1) {
            ::fcntl(STDIN_FILENO, F_SETFL, originalFlags_ | O_NONBLOCK);
            flagsChanged_ = true;
        }
    }

    void disableRawMode()
    {
        if (rawEnabled_) {
            ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTermios_);
        }

        if (flagsChanged_) {
            ::fcntl(STDIN_FILENO, F_SETFL, originalFlags_);
        }
    }

    termios originalTermios_ {};
    int originalFlags_ = -1;
    bool rawEnabled_ = false;
    bool flagsChanged_ = false;
};

} // namespace snakecraft

#endif
