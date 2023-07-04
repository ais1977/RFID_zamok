#include <Servo.h>
#include <EEPROM.h>
#include <OneWire.h>

const byte saveKey = 2;    // вход для кнопки обнуления
const byte openKey = 3;    // вход для кнопки открытия
const byte statusLed = 13; // led
const byte doorPin = 4;    // пин сервы
uint8_t door;

OneWire ds(8);
Servo servo;

byte addr[8];
byte allKey; // всего ключей

// функция сверяет два адреса (два массива)
boolean addrTest(byte addr1[8], byte addr2[8])
{
  for (int i = 0; i < 8; i++)
  {
    if (addr1[i] != addr2[i])
      return 0;
  }
  return 1;
}

void error()
{
  static uint8_t i = 5;
  while (i)
  {
    digitalWrite(statusLed, !digitalRead(statusLed));
    delay(300);
    i--;
  }
} //

boolean keyTest()
{ // возвращает 1 если ключ есть в еепром
  byte addrTemp[8];
  for (int i = 0; i < allKey; i++)
  {
    for (int y = 0; y < 8; y++)
      addrTemp[y] = EEPROM.read((i << 3) + y);
    if (addrTest(addrTemp, addr))
      return 1;
  }
  return 0;
} //

void save()
{ // сохраняет ключ в еепром
  digitalWrite(statusLed, HIGH);
  if (allKey >= 63)
    error(); // если места нет

  while (!ds.search(addr))
    ds.reset_search(); // ждем ключ
  if (OneWire::crc8(addr, 7) != addr[7])
    error();
  if (keyTest())
    error(); // если нашли ключ в еепром.

  for (int i = 0; i < 8; i++)
    EEPROM.write((allKey << 3) + i, addr[i]);

  allKey++; // прибавляем единицу к количеству ключей
  EEPROM.write(511, allKey);

  digitalWrite(statusLed, LOW);
} //

void openDoor()
{ // тут включаем\выключаем выход или крутим серву
  if (door)
  {
    servo.write(150);
    servo.detach();
    door = 0;
    delay(1000);
  }
  else
  {
    servo.write(1);
    servo.detach();
    door = 1;
    delay(1000);
  }
}

void setup()
{
  //  Serial.begin(9600);
  //  pinMode(doorPin, OUTPUT);
  pinMode(statusLed, OUTPUT);
  pinMode(saveKey, INPUT_PULLUP);
  pinMode(openKey, INPUT_PULLUP);
  servo.attach(doorPin);
  openDoor();
  // если при включении нажата кнопка, сбрасываем ключи на 0
  if (!digitalRead(saveKey))
    EEPROM.write(511, 0);

  allKey = EEPROM.read(511); // читаем количество ключей
}

void loop()
{
  if (!digitalRead(openKey))
    openDoor(); // открыть

  ds.reset_search();

  if (!digitalRead(saveKey))
    save(); // если нажали кнопку
  // сканируем шину, если нет устройств выходим из loop
  if (!ds.search(addr))
    return;
  if (OneWire::crc8(addr, 7) != addr[7])
    return;

  if (keyTest())
    openDoor(); // если нашли ключ в еепром, открываем дверь
}
