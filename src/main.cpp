#include <Arduino.h>

#include <Servo.h>   // Библиотека серво
#include <SPI.h>     // Библиотека SPI для MFRC522
#include <MFRC522.h> // Библиотека RFID модуля MFRC522
#include <EEPROM.h>  // Библиотека EEPROM для хранения ключей
#include "boot.h"    // key

#define LOCK_TIMEOUT 1000 // Время до блокировки замка после закрытия двери в мс
#define MAX_TAGS 50       // Максимальное количество хранимых меток - ключей
#define SERVO_PIN 2       // Пин серво
#define RST_PIN 6         // Пин RST MFRC522
#define CS_PIN 7          // Пин SDA MFRC522
#define BTN_PIN 8         // Пин кнопки
#define EE_START_ADDR 0   // Начальный адрес в EEPROM
#define EE_KEY 100        // Ключ EEPROM, для проверки на первое вкл.

MFRC522 rfid(CS_PIN, RST_PIN); // Обьект RFID
Servo doorServo;               // Обьект серво
key bat(BTN_PIN);              // Объект кнопки

void lock(void);
void unlock(void);
void saveOrDeleteTag(uint8_t *tag, uint8_t size);
bool compareUIDs(uint8_t *in1, uint8_t *in2, uint8_t size);
int16_t foundTag(uint8_t *tag, uint8_t size);

uint8_t locked = 1;    // Флаг состояния замка
bool needLock = false; // Служебный флаг
uint8_t savedTags = 0; // кол-во записанных меток

uint32_t lockTimeout; // Таймер таймаута кнопки
uint32_t rfidTimeout; // Таймер таймаута метки

void setup()
{
  // Инициализируем все
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

  // Настраиваем пины
  pinMode(BTN_PIN, INPUT_PULLUP);

  // Полная очистка при включении при зажатой кнопке
  uint32_t start = millis(); // Отслеживание длительного удержания кнопки после включения
  bool needClear = 0;        // Чистим флаг на стирание
  while (!digitalRead(BTN_PIN))
  { // Пока кнопка нажата
    if (millis() - start >= 3000)
    {                   // Встроенный таймаут на 3 секунды
      needClear = true; // Ставим флаг стирания при достижении таймаута
      break;            // Выходим из цикла
    }
  }

  // Инициализация EEPROM
  if (needClear or EEPROM.read(EE_START_ADDR) != EE_KEY)
  { // при первом включении или необходимости очистки ключей
    for (uint16_t i = 0; i < EEPROM.length(); i++)
      EEPROM.write(i, 0x00);             // Чистим всю EEPROM
    EEPROM.write(EE_START_ADDR, EE_KEY); // Пишем байт-ключ
  }
  else
  {                                             // Обычное включение
    savedTags = EEPROM.read(EE_START_ADDR + 1); // Читаем кол-во меток в памяти
  }

  // Начальное состояние замка
  locked = 0; // Замок открыт
  unlock();   // На всякий случай дернем замок
}

void loop()
{
  uint16_t b = bat.tik();
  // Закрытие по нажатию кнопки изнутри
  if (locked == false && b == 1)
  {                         // Если дверь открыта и нажали кнопку
    lock();                 // Блокируем
    locked = true;          // Замок закрыт
    lockTimeout = millis(); // Запомнили время
  }

  // Открытие по нажатию кнопки изнутри
  if (locked == true && b == 1)
  {                         // Если дверь закрыта и нажали кнопку
    unlock();               // Разблокируем замок
    locked = false;         // Замок открыт
    lockTimeout = millis(); // Запомнили время
  }

  // Поднесение метки
  if (rfid.PICC_IsNewCardPresent() and rfid.PICC_ReadCardSerial()) // Если поднесена карта
  {
    if (b == 100 and millis() - rfidTimeout >= 500)                // кнопка нажата
    {
      saveOrDeleteTag(rfid.uid.uidByte, rfid.uid.size);            // Сохраняем или удаляем метку
    }
    else
    {
      if (foundTag(rfid.uid.uidByte, rfid.uid.size) >= 0)          // Ищем метку в базе
      {
        if (millis() - lockTimeout >= LOCK_TIMEOUT && locked == false)
        {
          lock();                 // Блокируем
          locked = true;          // Замок закрыт
          lockTimeout = millis(); // Запомнили время
        }
        if (millis() - lockTimeout >= LOCK_TIMEOUT && locked == true)
        {
          unlock();               // Разблокируем замок
          locked = false;         // Замок открыт
          lockTimeout = millis(); // Запомнили время
        }
      }
    }
    rfidTimeout = millis(); // Обвновляем таймаут
  }

  // Перезагружаем RFID каждые 0.5 сек (для надежности)
  static uint32_t rfidRebootTimer = millis(); // Таймер
  if (millis() - rfidRebootTimer > 500)
  {                              // Каждые 500 мс
    rfidRebootTimer = millis();  // Обновляем таймер
    digitalWrite(RST_PIN, HIGH); // Дергаем резет
    delay(1);
    digitalWrite(RST_PIN, LOW);
    rfid.PCD_Init(); // Инициализируем модуль
  }
}

void lock(void)
{ // Функция должна блокировать замок или нечто иное
  doorServo.attach(SERVO_PIN);
  doorServo.write(170); // Для примера - запиранеие замка при помощи серво
  delay(1000);
  doorServo.detach(); // Детачим серво, чтобы не хрустела
  Serial.println("lock");
}

void unlock(void)
{ // Функция должна разблокировать замок или нечто иное
  doorServo.attach(SERVO_PIN);
  doorServo.write(10); // Для примера - отпирание замка при помощи серво
  delay(1000);
  doorServo.detach(); // Детачим серво, чтобы не хрустела
  Serial.println("unlock");
}

// Сравнение двух массивов известного размера
bool compareUIDs(uint8_t *in1, uint8_t *in2, uint8_t size)
{
  for (uint8_t i = 0; i < size; i++)
  { // Проходим по всем элементам
    if (in1[i] != in2[i])
      return false; // Если хоть один не сошелся - массивы не совпадают
  }
  return true; // Все сошлись - массивы идентичны
}

// Поиск метки в EEPROM
int16_t foundTag(uint8_t *tag, uint8_t size)
{
  uint8_t buf[8];   // Буфер метки
  uint16_t address; // Адрес
  for (uint8_t i = 0; i < savedTags; i++)
  {                                        // проходим по всем меткам
    address = (i * 8) + EE_START_ADDR + 2; // Считаем адрес текущей метки
    EEPROM.get(address, buf);              // Читаем метку из памяти
    if (compareUIDs(tag, buf, size))
      return address; // Сравниваем - если нашли возвращаем асдрес
  }
  return -1; // Если не нашли - вернем минус 1
}

// Удаление или запись новой метки
void saveOrDeleteTag(uint8_t *tag, uint8_t size)
{
  int16_t tagAddr = foundTag(tag, size);                     // Ищем метку в базе
  uint16_t newTagAddr = (savedTags * 8) + EE_START_ADDR + 2; // Адрес крайней метки в EEPROM
  if (tagAddr >= 0)
  {
    delay(10);
  }
  else if (savedTags < MAX_TAGS)
  { // метка не найдена - нужно записать, и лимит не достигнут
    for (uint16_t i = 0; i < size; i++)
      EEPROM.write(i + newTagAddr, tag[i]);       // Зная адрес пишем новую метку
    EEPROM.write(EE_START_ADDR + 1, ++savedTags); // Увеличиваем кол-во меток и пишем
  }
  else
  { // лимит меток при попытке записи новой
    delay(10);
  }
}
