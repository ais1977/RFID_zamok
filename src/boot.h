#include <Arduino.h>

class key
{

public:
    key(uint8_t pin);
    uint16_t tik();

private:
    uint8_t _bounceTime = 10;     // задержка для подавления дребезга
    uint16_t _holdTime = 500;     // время, в течение которого нажатие можно считать удержанием кнопки
    uint16_t _doubleTime = 400;   // время, в течение которого нажатия можно считать двойным
    uint16_t _holdTimeStep = 500; // время "тиков" при удержании кнопки
    uint16_t _holdTimeSum = 0;
    int8_t _i = 0;
    uint8_t _pin = 0;

    bool _lastReading = false; // флаг предыдущего состояния кнопки
    bool _buttonPress = false; // флаг состояния "краткое нажатие"
    bool _buttonHold = false;  // флаг состояния "долгое нажатие"
    bool _reading;

    uint32_t _onTime = 0;         // переменная обработки временного интервала
    uint32_t _lastSwitchTime = 0; // переменная времени предыдущего переключения состояния
};

key::key(uint8_t pin)
{
    _pin = pin;
    pinMode(pin, INPUT_PULLUP);
}

uint16_t key::tik()
{
    _reading = !digitalRead(_pin);

    if (_reading && !_lastReading)
    {
        _onTime = millis();
    }

    if (_reading && _lastReading) //////удержание
    {
        if ((millis() - _onTime) > _holdTime)
        {
            _holdTimeSum = (millis() - _onTime);
            _buttonHold = true;
        }
    }

    if (!_reading && _lastReading)
    {
        if (((millis() - _onTime) > _bounceTime) && !_buttonHold)
        {
            if ((millis() - _lastSwitchTime) >= _doubleTime)
            {
                _lastSwitchTime = millis();
                _buttonPress = true;
                _i = 1;
            }
            else
            {
                _i++;
                _lastSwitchTime = millis();
                _buttonPress = true;
            }
        }
    }

    _lastReading = _reading;

    if (_i < 10 && _buttonPress && (millis() - _lastSwitchTime) > _doubleTime)
    {
        _buttonPress = false;
        return _i;
    }

    if (_buttonHold)
    {
        _buttonPress = false;
        _buttonHold = false;
        _lastSwitchTime = millis();
        _i = -1;
        if (_holdTimeStep < _holdTimeSum)
        {
            _onTime = millis();
            return 100;
        }
    }

    return 0;
}