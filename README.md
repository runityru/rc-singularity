Библиотека для работы с разделяемыми хеш-таблицами (пары ключ + значение).

## Возможности:
1. Скорости чтения/записи < 500 тактов на ключ (5-15 млн. операций в секунду на современных CPU)
2. Небольшой расход памяти на ключ + значение (1-1.4 от объема исходных данных)
3. Поддержание актуальной копии данных на диске.
4. Ускоренные чтение и запись наборов ключей (несколько ключей вместе пишутся и читаются значительно быстрее чем по отдельности).
5. Одновременное использование данных несколькими процессами и несколькими потоками внутри одного процесса
6. Операции алгебры множеств над наборами ключей
7. Загрузка и выгрузка csv-подобных файлов
8. Частичная загрузка данных в память ("ленивый" старт с дисковой копии, возможность небольших апдейтов больших наборов данных с последующей выгрузкой из памяти без полной загрузки)

## Текущие ограничения (32-битная индексация):
1. Размер хеш-таблицы до 4 млрд (до 16 млрд. ключей без потери производительности)
2. Суммарный размер данных до 16 Гб (ключи + значения)
3. Расход памяти на набор 0.5-1 Мб (использование неэффективно для маленьких наборов данных)

## Приоритеты разработки:
1. Расход памяти на ключ
2. Скорость чтения
3. Скорость записи
4. Расход памяти на набор.

## Роадмап:
1. Снижение расхода памяти на набор
2. Повышение локальности данных для ускорения записи
3. Частичная выгрузка данных из памяти для полноценной работы в ограниченном объеме памяти
4. Версия с 64 битной индексацией

## sing_handler
Консольная утилита для работы с наборами и csv файлами. Устанавливается в /usr/local/nic/rc-singularity/bin, 
версия с внутренней диагностикой ошибок в /usr/local/nic/rc-singularity/debug  
Предоставляет доступ к большинству вызовов библиотеки, может использоваться как пример кода.  
Справка по ключам утилиты: sing_hander -h

[Описание API](API.md)  
[Примеры использования](https://gitlab.com/rucenter/rc-singularity/-/wikis/%D0%9F%D1%80%D0%B8%D0%BC%D0%B5%D1%80%D1%8B-%D0%B8%D1%81%D0%BF%D0%BE%D0%BB%D1%8C%D0%B7%D0%BE%D0%B2%D0%B0%D0%BD%D0%B8%D1%8F)
