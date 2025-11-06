# ООП в C

![Вот так, с помощью нехитрых приспособлений буханку хлеба можно превратить в троллейбус](bred.png)

## ООП
* инкапсуляция
* наследование
* полиморфизм

[Ben Klemens, 21st Century C: C Tips from the New School](https://www.amazon.com/21st-Century-Tips-New-School/dp/1449327141)

## Непрозрачный указатель

[Opaque pointer](https://en.wikipedia.org/wiki/Opaque_pointer)

[Incomplete types](https://en.cppreference.com/w/c/language/type#Incomplete_types)

```c
/* obj.h */
typedef struct obj_t obj_t;

obj_t* obj_alloc();

void obj_setid(obj_t*, int);

int obj_getid(obj_t*);

void obj_free(obj_t*);


/* obj.c */
#include "obj.h"

struct obj_t {
    int id;
};

obj_t* obj_alloc() {
    obj_t* result = (obj_t*)malloc(sizeof(obj_t));
    result->id = 0;
    return result;
}

void obj_setid(obj_t* o, int i) {
    o->id = i;
}

int obj_getid(obj_t* o) {
    return o->id;
}

void obj_free(obj_t* o) {
    free(o);
}


/* main.c */
#include "obj.h"

int main() {
    obj_t* obj = obj_alloc();
    obj_setid(obj, 42);
    printf("%d\n", obj_getid(obj));
    obj_free(obj);
    return 0;
}
```

[C Interfaces and Implementations: Techniques for Creating Reusable Software](https://www.amazon.com/Interfaces-Implementations-Techniques-Creating-Reusable/dp/0201498413)

## Function linkage

Стандарт, 6.2.2/1:

> An identifier declared in different scopes or in the same scope more than once can be made to refer to the same object or function by a process called linkage.
> There are three kinds of linkage: external, internal, and none.

6.2.2/2:

> In the set of translation units and libraries that constitutes an entire program, each declaration of a particular identifier with external linkage denotes the same object or function. Within one translation unit, each declaration of an identifier with internal linkage denotes the same object or function.

6.2.2/5:

> If the declaration of an identifier for a function has no storage-class specifier, its linkage is determined exactly as if it were declared with the storage-class specifier extern.

6.2.2/3:

> If the declaration of a file scope identifier for an object or a function contains the storage-class specifier static, the identifier has internal linkage.
> A function declaration can contain the storage-class specifier static only if it is at file scope.

[The C Book - Linkage](https://webhome.phy.duke.edu/~rgb/General/c_book/c_book/chapter4/linkage.html)

[Как использовать extern?](http://microsin.net/programming/arm-troubleshooting-faq/how-use-extern.html)

```c
/* header.h */

extern int public_func(int);

/* implementation.c */

#include "header.h"

static int private_func(int x)
{
    return x + 42;
}

int public_func(int x)
{
    return private_func(x) * 2;
}

/* main.c */

#include "header.h"

int main(int argc, char** argv)
{
    public_func(argc);
    // private_func(argc);
}
```

## C11 struct inheritance

```c
typedef struct point
{
    double x, y;
} point;

typedef struct
{
    struct point;
    double z;
} threepoint;

double threelength (threepoint p)
{
    return sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
}

int main()
{
    threepoint p = {.x=3, .y=0, .z=4};
    printf("p is %g units from the origin\n", threelength(p));
}
```

```sh
gcc -std=c11 -fms-extensions yourfile.c
```

## CPython PyObject inheritance

```c
/* CPython/Include/object.h */

typedef struct _object {
    PyObject_HEAD
} PyObject;

/* PyObject_HEAD defines the initial segment of every PyObject. */
#define PyObject_HEAD                   \
    _PyObject_HEAD_EXTRA                \
    Py_ssize_t ob_refcnt;               \
    struct _typeobject *ob_type;

/* CPython/Include/intobject.h */

typedef struct {
    PyObject_HEAD
    long ob_ival;
} PyIntObject;

PyAPI_FUNC(PyObject *) PyInt_FromLong(long);
```

## _Generic

```c
/* cplx.h */

#include <complex.h>
#include <gsl/gsl_vector.h>

gsl_vector_complex *cvec_dot_gslcplx(gsl_vector_complex *v, gsl_complex x);
gsl_vector_complex *vec_dot_gslcplx(gsl_vector *v, gsl_complex x);
gsl_vector_complex *cvec_dot_c(gsl_vector_complex *v, complex double x);
gsl_vector_complex *vec_dot_c(gsl_vector *v, complex double x);
void gsl_vector_complex_print(gsl_vector_complex *v);

#define gsl_cplx_from_c99(x) (gsl_complex){.dat= {creal(x), cimag(x)}}

complex double ddot (complex double x, complex double y);

#define dot(x,y) _Generic((x),                                 \
                   gsl_vector*: dot_given_vec(y),              \
                   gsl_vector_complex*: dot_given_cplx_vec(y), \
                   default: ddot) ((x),(y))

#define dot_given_vec(y) _Generic((y),                \
                        gsl_complex: vec_dot_gslcplx, \
                        default: vec_dot_c)

#define dot_given_cplx_vec(y) _Generic((y),            \
                        gsl_complex: cvec_dot_gslcplx, \
                        default: cvec_dot_c)

/* main.c */

#include "cplx.h"

int main()
{
    int complex a = 1+2I;
    complex double b = 2+I;
    gsl_complex c = gsl_cplx_from_c99(a);
    double d = 2;

    gsl_vector *v = gsl_vector_alloc(8);
    for(int i=0; i< v->size; i++) gsl_vector_set(v, i, i/8.);

    complex double adotb = dot(a, b);

    gsl_vector_complex *vc = dot(v, a);

    gsl_vector_complex_print(dot(v, c));

    gsl_vector_complex_print(dot(v, d));
}
```

## Реализации ООП в C

[FAR manager plugins](https://api.farmanager.com/ru/index.html)

[C Object System](https://github.com/CObjectSystem/COS)

[Axel-Tobias Schreiner, Object-Oriented Programming With ANSI-C](https://www.cs.rit.edu/~ats/books/ooc.pdf), [перевод](http://dmilvdv.narod.ru/Translate/OOC/index.html)

[Object-oriented design patterns in the kernel, part 1](https://lwn.net/Articles/444910), [part 2](https://lwn.net/Articles/446317)

## GLib

[GLib](https://docs.gtk.org/glib), [GLib Refernce Manual](https://web.archive.org/web/20240105013344/https://developer-old.gnome.org/glib/stable/)

* [GObject](https://web.archive.org/web/20240105013344/https://developer-old.gnome.org/gobject/stable)
* [GLib](https://web.archive.org/web/20240105013344/https://developer-old.gnome.org/glib/stable/glib-fundamentals.html)
* [GModule](https://web.archive.org/web/20240105013344/https://developer-old.gnome.org/glib/stable/glib-Dynamic-Loading-of-Modules.html)
* [GThread](https://web.archive.org/web/20240105013344/https://developer-old.gnome.org/glib/stable/glib-Threads.html)
* [GIO](https://web.archive.org/web/20240105013344/https://developer-old.gnome.org/gio/stable)
* [Data types](https://web.archive.org/web/20240105013344/https://developer-old.gnome.org/glib/stable/glib-data-types.html)

[Object memory management](https://web.archive.org/web/20240105013344/https://developer-old.gnome.org/gobject/stable/gobject-memory.html)

[Цикл статей по GObject](https://habr.com/ru/post/348204), [2 часть](https://habr.com/ru/post/350132), [3 часть](https://habr.com/ru/post/418443)

```c
/* animalcat.h */

#define ANIMAL_TYPE_CAT animal_cat_get_type()
G_DECLARE_DERIVABLE_TYPE (AnimalCat, animal_cat, ANIMAL, CAT, GObject)

struct _AnimalCatClass
{
    GObjectClass parent_class;
    void (*say_meow)(AnimalCat*);
};

AnimalCat* animal_cat_new();
void animal_cat_say_meow(AnimalCat* self);


/* animalcat.c */

#include "animalcat.h"

G_DEFINE_TYPE (AnimalCat, animal_cat, G_TYPE_OBJECT)

//struct _AnimalCat
//{
//    GObject parent;
//};

static void animal_cat_init(AnimalCat* self)
{
    printf("Kitty was born.\n");
}

static void animal_cat_real_say_meow(AnimalCat* self)
{
    printf("Cat say: MEOW!\n");
}

void animal_cat_say_meow(AnimalCat* self)
{
    AnimalCatClass* klass = ANIMAL_CAT_GET_CLASS (self);
    klass->say_meow(self);
}

static void animal_cat_class_init(AnimalCatClass* self)
{
    printf("First instance of AnimalCat was created\n");
    self->say_meow = animal_cat_real_say_meow;
}

AnimalCat* animal_cat_new()
{
    return g_object_new(ANIMAL_TYPE_CAT, NULL);
}


/* main.c */

#include "animalcat.h"

int main(int argc, char** argv)
{
    AnimalCat* cat = animal_cat_new();
    animal_cat_say_meow(cat);
}
```

```shell
gcc main.c animalcat.c $(pkg-config --cflags --libs glib-2.0)
```

### GTK

[The GTK Project](https://gtk.org)

[Getting Started with GTK](https://docs.gtk.org/gtk4/getting_started.html)

![](gtk.png)

![](gdk.png)

```c
#include <gtk/gtk.h>

static void on_activate (GtkApplication *app)
{
    GtkWidget* window = gtk_application_window_new (app);
    GtkWidget* button = gtk_button_new_with_label ("Hello, World!");

    g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_window_close), window);
    gtk_window_set_child (GTK_WINDOW (window), button);
    gtk_window_present (GTK_WINDOW (window));
}

int main (int argc, char** argv)
{
    GtkApplication* app = gtk_application_new ("com.example.GtkApplication",
                                               G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect (app, "activate", G_CALLBACK (on_activate), NULL);
    return g_application_run (G_APPLICATION (app), argc, argv);
}
```

```shell
gcc example.c $(pkg-config --cflags --libs gtk4)
```

[Canonical launches Ubuntu Frame, the foundation for embedded displays](https://ubuntu.com/blog/canonical-launches-ubuntu-frame-the-foundation-for-embedded-displays)

[Mir Server | Ubuntu Frame](https://mir-server.io/ubuntu-frame)

### GStreamer

[GStreamer: open source multimedia framework](https://gstreamer.freedesktop.org)

![Typical GStreamer pipeline](pipeline.png)

[Знакомство с GStreamer](https://habr.com/ru/post/178813), [2 часть](https://habr.com/ru/post/179167), [3 часть](https://habr.com/ru/post/204014)

* v0.10 - legacy
* v1.x - stable ([Keynote](https://gstconf.ubicast.tv/videos/keynote-gstreamer-10), [Porting 0.10 to 1.0](https://gstreamer.freedesktop.org/documentation/application-development/appendix/porting-1-0.html?gi-language=c))

---

* `gcc -shared -fPIC wavsource.c $(pkg-config --cflags --libs gstreamer-1.0) -o libwavsource.so`
* `gst-launch`

  ```sh
  gst-launch-1.0 -v v4l2src device=/dev/video0 ! video/x-raw,framerate=30/1 ! xvimagesink
  ```
* `gst-inspect`

  ```sh
  gst-inspect-1.0 xvimagesink
  ```
* `GST_DEBUG`, `GST_PLUGIN_PATH`
  ```sh
  GST_DEBUG=4 GST_PLUGIN_PATH=`pwd` gst-launch-1.0 myplugin location=test.wav ! autoaudiosink
  ```
* [gst-element-maker](https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/-/blob/master/tools/gst-element-maker)

#### GStreamer plugins

[Plugin Writer's Guide](https://gstreamer.freedesktop.org/documentation/plugin-development/?gi-language=c)

Имя плагина должно совпадать с именем динамической библиотеки: [Plugin loading failing](https://gstreamer-devel.narkive.com/QH9Qcupf/plugin-loading-failing-with-gstreamer-1-14-0)

[ООП на C: пишем видеоплеер](https://youtu.be/eCJT7id50V8) ([расшифровка](https://otus.ru/journal/oop-na-c-pishem-videopleer)), [otus-video-player source](https://gitlab.com/lockie/otus-video-player)
