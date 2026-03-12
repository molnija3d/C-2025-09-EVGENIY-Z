import os
import ycm_core

# Определяем базовую директорию (там, где лежит этот .ycm_extra_conf.py)
dir_of_script = os.path.dirname(os.path.abspath(__file__))

# Флаги, которые будут переданы компилятору (clang)
flags = [
    # Язык: C (не C++)
    '-x', 'c',
    # Стандарт языка (выберите подходящий: c99, c11, gnu11 и т.д.)
    '-std=c11',
    # Пути к заголовочным файлам (добавьте свои)
    '-I', os.path.join(dir_of_script, 'headers'),
    '-I', os.path.join(dir_of_script, 'src'),
    # Иногда полезно добавить саму директорию проекта
    '-I', dir_of_script,
    # Дополнительные флаги (опционально)
    '-Wall',
    '-Wextra',
]

# Опционально: если вы используете системные библиотеки (например, для Linux)
# flags.append('-isystem/usr/include')

def Settings(**kwargs):
    # Эта функция вызывается YCM для получения настроек.
    # Просто возвращаем наш список флагов.
    return {'flags': flags}
