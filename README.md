# Интерпретатор языка mython
## Описание проекта
_Обучающий проект_. Интерпретатор программ на языке программирования, представляющем собой урезанную версию Python. 

Mython поддерживает:
- только целые числа, строки ("...",'...'), логические константы (True, False), None;
- однострочные комментарии (#...);
- идентификаторы (как в Python);
- создание классов (возможно объявление специальных методов `__init__`, `__add__`, `__lt__`, `__eq__`, `__str__`, работает указатель на объект self);
- преобразование объекта в строку (str(x));
- печать в поток вывода (print);
- арифметика целых чисел (+, -, *, / - работает как //) и унарный минус;
- логические операции (and, or, not);
- конкатенация строк (+);
- операции сравнения для строк и целых чисел;
- условный оператор;
- наследование;
- определение методов (def).

## Использующиеся технологии
- лямбда-функции,
- variadic templates,
- forwarding references,
- объектно-ориентированное проектирование.
  
## Модули
`lexer` - лексический анализатор, разбивает код на лексемы.

`parse` - синтаксический анализатор, разбирает структруру кода.

`runtime` - описывает все сущности языка.

`statement` - описывает все выполняемые (Executable) сущности языка и как они работают.

`test_runner_p` - фреймворк для запуска unit-тестов.

## Системные требования
С++17
