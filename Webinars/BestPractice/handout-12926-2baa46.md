# Best practices программирования на С

## Документы
* [ISO/IEC TS 17961](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1624.pdf)
* [SEI CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard)
* [MISRA C](https://misra.org.uk)
* [NASA C style guide](https://ntrs.nasa.gov/api/citations/19950022400/downloads/19950022400.pdf), [Mars Code](https://cacm.acm.org/magazines/2014/2/171689-mars-code/fulltext)
* [Micriµm C Coding Standard](https://d1.amobbs.com/bbs_upload782111/files_38/ourdev_630682QLHUQ2.pdf)
* [Steve Oualline. C Elements of Style: The Programmer's Style Manual for Elegant C and C++ Programs](http://www.oualline.com/books.free/style/index.html)
* [Марсокод, или Как создавалось ПО для марсохода Curiosity](https://devby.io/news/marsokod-ili-kak-sozdavalos-po-dlya-marsohoda-curiosity)
* [47 Атрибутов Хорошего С-кода](https://habr.com/ru/post/679256)
* [Writing Safer C code](https://github.com/Cyan4973/Writing_Safer_C_code)
* **Worst practice**: "C с классами". [Orthodox C++](https://gist.github.com/bkaradzic/2e39896bc7d8c34e042b). [Modern C for C++ Peeps](https://floooh.github.io/2019/09/27/modern-c-for-cpp-peeps.html).

### Style guides
* [Linux kernel coding style](https://www.kernel.org/doc/html/v4.19/process/coding-style.html)
  * [Kernel Coding the Upstream Way - Tim Bird, Sony](https://youtu.be/dApfkuObBW8)
* [OpenSSL coding style](https://openssl.org/policies/technical/coding-style.html)
* [FreeBSD kernel style guide](https://freebsd.org/cgi/man.cgi?query=style&sektion=9)
* [GNU coding standards](https://gnu.org/prep/standards/standards.html#Writing-C)
* [Apache Developers' C Language Style Guide](https://httpd.apache.org/dev/styleguide.html)
* [OpenResty C Coding Style Guide](https://openresty.org/en/c-coding-style-guide.html), [ngx-releng](https://github.com/openresty/openresty-devel-utils/blob/master/ngx-releng)

### Автоматические форматтеры кода
* [GNU indent](https://gnu.org/software/indent/manual/indent.html)
* [Uncrustify](http://uncrustify.sourceforge.net)
  * [UncrustifyConfig](https://cdanu.github.io/uncrustify_config_preview/index.html)
* [clang-format](https://clang.llvm.org/docs/ClangFormat.html)
  * [clang-format configurator](https://zed0.co.uk/clang-format-configurator)

## Best practices

### Избегаем UB

MISRA Rule 1.2: No reliance shall be placed on undefined or unspecified behaviour.

![Thank you, captain Obvious!](obvious.png)

* целочисленные переполнения
  ```c
  int64_t bill_gates_fortune;
  for(size_t i = 0; i < strlen(str); i++);
  ```
* неинициализированные переменные
  ```c
  int i;
  printf("%d\n", i);

  char* str = malloc(4096);
  printf("%s\n", str);
  ```
  [Debian and OpenSSL: The Aftermath](http://web.archive.org/web/20221006171031/https://www.links.org/?p=328), [Why is uninitialized memory safe to use in OpenSSL's random number generator? - Stack Overflow](https://stackoverflow.com/a/45399290/1336774)
* ошибки доступа к памяти
  * доступ к уже удалённой через `free` динамической памяти
  * доступ к переменной на стеке после возврата из родительской функции
  * доступ за пределами памяти
    * [Stack buffer overflow - Wikipedia](https://en.wikipedia.org/wiki/Stack_buffer_overflow)
    * [2021 CWE Top 25 Most Dangerous Software Weaknesses](https://cwe.mitre.org/top25/archive/2021/2021_cwe_top25.html)
    * [Использование подсказок, включаемых в исходный код, помогающих GCC выявлять случаи переполнения буфера](https://habr.com/ru/company/ruvds/blog/572338)
  * [утечка памяти](https://ru.wikipedia.org/wiki/Утечка_памяти)
    * [Pointers and memory leaks in C](https://developer.ibm.com/articles/au-toughgame)

```
-Wl,-z,noexecstack
```

#### Bounds-checking interfaces

* [ISO/IEC TR 24731-1](http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1225.pdf), C11 Annex K

```c
// #ifdef __STDC_LIB_EXT1__
// #define __STDC_WANT_LIB_EXT1__
errno_t strcpy_s(char* restrict s1,
                 rsize_t s1max,
                 const char* restrict s2);
```

> This function or variable may be unsafe. Consider using safe-version instead. To disable deprecation, use _CRT_SECURE_NO_WARNINGS. See online help for details.

-- MSVC

[Security Features in the CRT | Microsoft Docs](https://docs.microsoft.com/en-us/cpp/c-runtime-library/security-features-in-the-crt)

* [`strncpy`](https://en.cppreference.com/w/c/string/byte/strncpy), [`strncat`](https://en.cppreference.com/w/c/string/byte/strncat). [Безопасность](https://ru.wikipedia.org/wiki/Strncpy#Безопасность)
* [`strlcpy`, `strlcat`](https://www.freebsd.org/cgi/man.cgi?query=strlcpy&sektion=3) (BSD, [source](http://cvsweb.openbsd.org/cgi-bin/cvsweb/src/lib/libc/string/strlcpy.c?rev=1.16&content-type=text/x-cvsweb-markup))
* [`strnlen`](https://man7.org/linux/man-pages/man3/strnlen.3.html)
* [`strerror_r`](https://man7.org/linux/man-pages/man3/strerror.3.html)
* [`strtok_r`](https://man7.org/linux/man-pages/man3/strtok.3.html)
* [`fgets`](https://en.cppreference.com/w/c/io/fgets)
* [`ctime_r`, `gmtime_r`, `asctime_r`, `localtime_r`](https://man7.org/linux/man-pages/man3/ctime.3.html)
* [`snprintf`](https://en.cppreference.com/w/c/io/fprintf), [`vsnprintf`](https://en.cppreference.com/w/c/io/vfprintf)
* [`asprintf`, `vasprintf`](https://man7.org/linux/man-pages/man3/asprintf.3.html)
* [Обнаружено еще одно имя сети Wi-Fi, выводящее из строя iPhone](https://securitylab.ru/news/521934.php)
  ```c
  // Bad
  printf(user_str);
  // Good
  printf("%s", user_str);
  // Good
  puts(user_str);
  ```

#### Статические анализаторы кода
* [Lint](https://en.wikipedia.org/wiki/Lint_(software))
* [Clang Static Analyzer](https://clang-analyzer.llvm.org), [clang-tidy](https://clang.llvm.org/extra/clang-tidy)
* [Cppcheck](http://cppcheck.sourceforge.net)
* [PVS studio](https://pvs-studio.com/ru/pvs-studio)
  * [блог на Хабре](https://habr.com/ru/company/pvs-studio)
* [Intel ControlFlag](https://github.com/IntelLabs/control-flag)

[John Carmack. In-Depth: Static Code Analysis](https://gamedeveloper.com/programming/in-depth-static-code-analysis)

[Тестирование СУБД: 10 лет опыта](https://habr.com/ru/company/vk/blog/584864)

#### Compiler Sanitizers

* [Compiler Sanitizers - HPC Wiki](https://hpc-wiki.info/hpc/Compiler_Sanitizers)
* Undefined Behaviour Sanitizer

  [Instrumentation Options (Using GCC)](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html#index-fsanitize_003dundefined)

    ```
    $ cat test.c
    int main(int argc, char **argv) {
        int k = 0x7fffffff;
        k += argc;
        return 0;
    }
    $ gcc -fsanitize=undefined test.c
    $ ./a.out
    test.c:3:5: runtime error: signed integer overflow: 2147483647 + 1 cannot
    be represented in type 'int'
    ```
* Address Sanitizer

  [AddressSanitizer · google/sanitizers Wiki](https://github.com/google/sanitizers/wiki/AddressSanitizer)

  ```
  $ cat test.c
  #include <stdlib.h>
  int main() {
    char *x = malloc(10 * sizeof(char));
    free(x);
    return x[5];
  }
  $ gcc -fsanitize=address test.c
  $ ./a.out
  ==25480==ERROR: AddressSanitizer: heap-use-after-free on address 0x60700000dfb5 at
  pc 0x0000004007d4 bp 0x7fff7645f390 sp 0x7fff7645f380
  ```

#### Defensive programming

* [Defensive programming - Wikipedia](https://en.wikipedia.org/wiki/Defensive_programming)
* [`assert`](https://en.cppreference.com/w/c/error/assert)
  ```c
  int process_string(char* str)
  {
      assert(str != NULL);
      // ...
  }

  // Bad
  assert(str && str[0] && strlen(str) > nSize);
  // Good
  assert(str != NULL);
  assert(str[0] != 0);
  assert(strlen(str) > nSize);

  // Bad
  assert(0);
  // Good
  assert(!"This should never happen, contact devs");

  // Bad
  assert(str != NULL);
  puts(str);

  // Good
  assert(str != NULL);
  if(str == NULL)
      return NULL;
  puts(str);
  ```
* проверка возвращаемых значений (`-Wunused-result`, `FORTIFY_SOURCE`)
  ```c
  char* str = malloc(4096);
  if(!str)
  {
      perror("malloc");
      exit(EXIT_FAILURE);
  }
  ```
* [`FORTIFY_SOURCE`](https://man7.org/linux/man-pages/man7/feature_test_macros.7.html#:~:text=_FORTIFY_SOURCE) 1/2 в GCC старше 4.0. [Enhance application security with FORTIFY_SOURCE](https://access.redhat.com/blogs/766093/posts/1976213), [перевод](https://habr.com/ru/company/otus/blog/709660)
  ```sh
  gcc source.c -O1 -D_FORTIFY_SOURCE=2
  ```
* журналирование aka logging


### Именование

```c
// Константы, макросы
#define MY_CONSTANT 42
#define MY_MACRO(x) ((x)*(x))

// Элементы перечислений
enum color
{
    RED,
    GREEN,
    BLUE
};

// Типы
typedef struct
{
    int value;
} data;
data d;

// Переменные
int my_var;
for(int i = 0; i < size; i++);

// Функции
// module-object-operation
int os_semaphore_post(void);
```

[Венгерская нотация - Википедия](https://ru.wikipedia.org/wiki/Венгерская_нотация)

```c
wc.lpfnWndProc   = WindowProc;
wc.hInstance     = hInstance;
wc.lpszClassName = CLASS_NAME;
```

> Этот стиль выбора имён называется «венгерской» записью по названию родины руководителя отдела программирования Microsoft Чарльза Симони, который его изобрёл. (А не потому, что его использование придаёт программам такой вид, будто они написаны на венгерском языке).

-- [А. Голуб. Верёвка достаточной длины, чтобы выстрелить себе в ногу](http://e-maxx.ru/bookz/files/golub_cord.pdf)

#### Лимиты

* C17 5.2.4.1:
  * 63 significant initial characters in an internal identifier or a macro name
  * 31 significant initial characters in an external identifier
  * 511 identifiers with block scope declared in one block
  * 4095 external identifiers in one translation unit
  * 127 nesting levels of blocks
  * 127 parameters in one function/macro
  * 4095 characters in a logical source line
  * 4095 characters in a string literal
  * 1023 members in a single structure or union
  * 1023 enumeration constants in a single enumeration

[SEI CERT DCL40-C](https://wiki.sei.cmu.edu/confluence/display/c/DCL40-C.+Do+not+create+incompatible+declarations+of+the+same+function+or+object)

#### Зарезервированные имена

[SEI CERT DCL37-C](https://wiki.sei.cmu.edu/confluence/display/c/DCL37-C.+Do+not+declare+or+define+a+reserved+identifier)

* [Reserved Names (The GNU C Library)](https://gnu.org/software/libc/manual/html_node/Reserved-Names.html)

> The names of all library types, macros, variables and functions that come from the ISO C standard are reserved unconditionally; your program may not redefine these names. All other library names are reserved if your program explicitly includes the header file that defines or declares them.

> reserved names include all external identifiers (global functions and variables) that begin with an underscore (‘_’) and all identifiers regardless of use that begin with either two underscores or an underscore followed by a capital letter

### Комментарии

* [Code Tells You How, Comments Tell You Why](https://blog.codinghorror.com/code-tells-you-how-comments-tell-you-why)
* `#if 0`
* `#endif  // ...`

### Декомпозиция и возможности языка

[C Interfaces and Implementations](http://drh.github.io/cii/index.html)

* `*.c` + `*.h` = "модуль"
  * [Google C++ Style Guide: Names and Order of Includes](https://google.github.io/styleguide/cppguide.html#Names_and_Order_of_Includes)
* "include guards":
  ```c
  #ifndef MYFILE_H
  #define MYFILE_H
  // ...
  #endif  // MYFILE_H
  ```
* сокрытие: `extern` vs `static`, opaque pointer
* `const`
  ```c
  char* const a;  // pointer is constant, the data is not
  const char* a;  // pointed data cannot be written using pointer
  ```
* modern features: `bool`, VLA, designated initializers, generics, compound literals etc.
  ```c
  // Bad
  int v = 1;
  setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof v );

  // Good
  setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &( int ){ 1 }, sizeof int );
  ```
* [GOTOphobia considered harmful (in C)](https://jorengarenar.github.io/blog/gotophobia-harmful), [О вреде GOTO-фобии (с примерами на C)](https://habr.com/ru/articles/720486)
