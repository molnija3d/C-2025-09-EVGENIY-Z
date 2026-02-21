#include <GL/glut.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TEX_CUBE_FILE "earth.bmp"
#define TEX_SPHERE_FILE "moon.bmp"
#define M_PI 3.14159265354

/* Глобальный переменный для поворота куба и сферы*/
static float cubeAngle = 0.0f;
static float orbitAngle = 0.0f;     // положение сферы на орбите
static float sphereRotAngle = 0.0f; // собственное вращение сферы
static GLuint texCube, texSphere;   // идентификаторы текстур

/* Параметры орбиты */
const float ORBIT_RADIUS = 2.0f;
const float ORBIT_SPEED = 0.02f;     // прирост угла за кадр
const float SPHERE_ROT_SPEED = 0.5f; // скорость вращения сферы

/* Создание текстуры "шахматная доска" */
GLuint createCheckerboardTexture() {
    GLuint texture;
    int width = 64;
    int height = 64;
    GLubyte *data = malloc(width * height * 3); /* RGB */
    int i, j;

    /* Заполняем данные текстуры: чередующиеся чёрные и белые квадраты */
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            int c = ((((i & 0x8) == 0) ^ ((j & 0x8) == 0))) * 255;
            data[(i * width + j) * 3 + 0] = (GLubyte) c;
            data[(i * width + j) * 3 + 1] = (GLubyte) c;
            data[(i * width + j) * 3 + 2] = (GLubyte) c;
        }
    }

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    free(data);
    return texture;
}

/* Загрузка текстуры из файла */
GLuint loadTextureFromFile(const char *filename) {
    GLuint texture;
    int width, height, channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 0);
    if (!data) {
        fprintf(stderr, "Ошибка загрузки текстуры: %s\n", filename);
        return 0;
    }

    /* Определяем формат пикселей (RGB или RGBA) */
    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    /* Загружаем данные в OpenGL */
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    /* Устанавливаем параметры фильтрации и повторения */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(data);  /* Освобождаем память изображения */
    return texture;
}

/* Инициализация OpenGL */
void initOpenGL() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_TEXTURE_2D);

    /* Параметры источника света */
    GLfloat light_position[] = { 1.0, 0.0, 1.0, 0.0 };
    GLfloat white_light[] = { 1.0, 1.0, 1.0, 1.0 };
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, white_light);

    /* Материал: белый диффузный, чтобы текстура проявилась */
    GLfloat mat_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);

    /* Загружаем текстуры из файла  */
    texCube = loadTextureFromFile("earth.bmp");
    texSphere = loadTextureFromFile("moon.bmp");

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
}

/* Функция рисования куба */
void drawCube() {
    glBindTexture(GL_TEXTURE_2D, texCube);
    glBegin(GL_QUADS);
    {
        /* Передняя грань */
        glNormal3f(0,0,1);
        glTexCoord2f(0,0);
        glVertex3f(-0.5,-0.5,0.5);
        glTexCoord2f(1,0);
        glVertex3f(0.5,-0.5,0.5);
        glTexCoord2f(1,1);
        glVertex3f(0.5,0.5,0.5);
        glTexCoord2f(0,1);
        glVertex3f(-0.5,0.5,0.5);

        /* Задняя грань */
        glNormal3f(0,0,-1);
        glTexCoord2f(0,0);
        glVertex3f(-0.5,-0.5,-0.5);
        glTexCoord2f(1,0);
        glVertex3f(-0.5,0.5,-0.5);
        glTexCoord2f(1,1);
        glVertex3f(0.5,0.5,-0.5);
        glTexCoord2f(0,1);
        glVertex3f(0.5,-0.5,-0.5);

        /* Левая грань */
        glNormal3f(-1,0,0);
        glTexCoord2f(0,0);
        glVertex3f(-0.5,-0.5,-0.5);
        glTexCoord2f(1,0);
        glVertex3f(-0.5,-0.5,0.5);
        glTexCoord2f(1,1);
        glVertex3f(-0.5,0.5,0.5);
        glTexCoord2f(0,1);
        glVertex3f(-0.5,0.5,-0.5);

        /* Правая грань*/
        glNormal3f(1,0,0);
        glTexCoord2f(0,0);
        glVertex3f(0.5,-0.5,-0.5);
        glTexCoord2f(1,0);
        glVertex3f(0.5,0.5,-0.5);
        glTexCoord2f(1,1);
        glVertex3f(0.5,0.5,0.5);
        glTexCoord2f(0,1);
        glVertex3f(0.5,-0.5,0.5);

        /* Нижняя грань */
        glNormal3f(0,-1,0);
        glTexCoord2f(0,0);
        glVertex3f(-0.5,-0.5,-0.5);
        glTexCoord2f(1,0);
        glVertex3f(0.5,-0.5,-0.5);
        glTexCoord2f(1,1);
        glVertex3f(0.5,-0.5,0.5);
        glTexCoord2f(0,1);
        glVertex3f(-0.5,-0.5,0.5);

        /* Верхняя грань */
        glNormal3f(0,1,0);
        glTexCoord2f(0,0);
        glVertex3f(-0.5,0.5,-0.5);
        glTexCoord2f(1,0);
        glVertex3f(-0.5,0.5,0.5);
        glTexCoord2f(1,1);
        glVertex3f(0.5,0.5,0.5);
        glTexCoord2f(0,1);
        glVertex3f(0.5,0.5,-0.5);
    }
    glEnd();
}

/*  Функция рисования сферы с текстурой */
void drawSphere() {
    glBindTexture(GL_TEXTURE_2D, texSphere);
    GLUquadric *quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);   /* автоматически генерировать текстурные координаты */
    gluQuadricNormals(quad, GLU_SMOOTH); /* сглаженные нормали */
    gluSphere(quad, 0.5, 32, 32);       /* радиус 0.5, 32 сегмента по широте и долготе */
    gluDeleteQuadric(quad);
}

/* Рисуем орбиту */
void drawOrbit() {

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glColor3f(1, 1, 1);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 360; i++) {
        float angle = i * M_PI / 180.0f;
        float x = ORBIT_RADIUS * cos(angle);
        float z = ORBIT_RADIUS * sin(angle);
        glVertex3f(x, 0, z);
    }
    glEnd();
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
}
/* Обработчик перерисовки окна */
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gluLookAt(3.0, 2.0, 5.0,   /* позиция камеры */
              0.0, 0.0, 0.0,   /* точка, на которую смотрим */
              0.0, 1.0, 0.0);  /* направление "вверх" */

    /* Рисуем куб (шесть граней) */
    glPushMatrix();             /* сохраняем текущую матрицу (вид) */
    glRotatef(cubeAngle, 1.0, 1.0, 0.0); /* вращаем куб */
    drawCube();
    glPopMatrix();              /* восстанавливаем матрицу */

    /* Рисуем орбиту (белая линия) */
    drawOrbit();
    /* Рисуем сферу на орбите */

    glPushMatrix();
    /* Перенос в точку на орбите (вычисляем координаты) */
    float x = ORBIT_RADIUS * cos(orbitAngle);
    float z = ORBIT_RADIUS * sin(orbitAngle);
    glTranslatef(x, 0.0f, z);

    /* Собственное вращение сферы (вокруг своей оси Y) */
    glRotatef(sphereRotAngle, 0.0, 1.0, 0.0);

    /* Рисуем сферу */
    drawSphere();
    glPopMatrix();

    glutSwapBuffers();
}

/* Обработчик изменения размеров окна */
void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)w / (double)h, 1.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

/* Обработчик клавиатуры */
void keyboard(unsigned char key, __attribute__((unused)) int x, __attribute__((unused)) int y) {
    if (key == 27) /* ESC */
        exit(0);
}

/* Функция таймера для анимации */
void timer(__attribute__((unused)) int value) {
    cubeAngle += 1.0f;
    if (cubeAngle > 360.0f) cubeAngle -= 360.0f;

    orbitAngle += ORBIT_SPEED;
    if (orbitAngle > 2 * M_PI) orbitAngle -= 2 * M_PI;

    sphereRotAngle += SPHERE_ROT_SPEED;
    if (sphereRotAngle > 360.0f) sphereRotAngle -= 360.0f;

    glutPostRedisplay();
    glutTimerFunc(16, timer, 0); /* примерно 60 кадров в секунду */
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("И невозможное возможно =). ESC для выхода.");

    initOpenGL();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(0, timer, 0);

    glutMainLoop();
    return 0;
}
