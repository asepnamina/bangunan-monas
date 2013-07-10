#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#include <GL/gl.h>
#include "imageloader.h"
#include "vec3f.h"
#endif

static GLfloat spin, spin2 = 0.0;
float angle = 0;
using namespace std;

float lastx, lasty;
GLint stencilBits;
static int viewx = 50;
static int viewy = 24;
static int viewz = 80;

float rot = 0;

//train 2D
//class untuk terain 2D
class Terrain {
private:
	int w; //Width
	int l; //Length
	float** hs; //Heights
	Vec3f** normals;
	bool computedNormals; //Whether normals is up-to-date
public:
	Terrain(int w2, int l2) {
		w = w2;
		l = l2;

		hs = new float*[l];
		for (int i = 0; i < l; i++) {
			hs[i] = new float[w];
		}

		normals = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals[i] = new Vec3f[w];
		}

		computedNormals = false;
	}

	~Terrain() {
		for (int i = 0; i < l; i++) {
			delete[] hs[i];
		}
		delete[] hs;

		for (int i = 0; i < l; i++) {
			delete[] normals[i];
		}
		delete[] normals;
	}

	int width() {
		return w;
	}

	int length() {
		return l;
	}

	//Sets the height at (x, z) to y
	void setHeight(int x, int z, float y) {
		hs[z][x] = y;
		computedNormals = false;
	}

	//Returns the height at (x, z)
	float getHeight(int x, int z) {
		return hs[z][x];
	}

	//Computes the normals, if they haven't been computed yet
	void computeNormals() {
		if (computedNormals) {
			return;
		}

		//Compute the rough version of the normals
		Vec3f** normals2 = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals2[i] = new Vec3f[w];
		}

		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum(0.0f, 0.0f, 0.0f);

				Vec3f out;
				if (z > 0) {
					out = Vec3f(0.0f, hs[z - 1][x] - hs[z][x], -1.0f);
				}
				Vec3f in;
				if (z < l - 1) {
					in = Vec3f(0.0f, hs[z + 1][x] - hs[z][x], 1.0f);
				}
				Vec3f left;
				if (x > 0) {
					left = Vec3f(-1.0f, hs[z][x - 1] - hs[z][x], 0.0f);
				}
				Vec3f right;
				if (x < w - 1) {
					right = Vec3f(1.0f, hs[z][x + 1] - hs[z][x], 0.0f);
				}

				if (x > 0 && z > 0) {
					sum += out.cross(left).normalize();
				}
				if (x > 0 && z < l - 1) {
					sum += left.cross(in).normalize();
				}
				if (x < w - 1 && z < l - 1) {
					sum += in.cross(right).normalize();
				}
				if (x < w - 1 && z > 0) {
					sum += right.cross(out).normalize();
				}

				normals2[z][x] = sum;
			}
		}

		//Smooth out the normals
		const float FALLOUT_RATIO = 0.5f;
		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum = normals2[z][x];

				if (x > 0) {
					sum += normals2[z][x - 1] * FALLOUT_RATIO;
				}
				if (x < w - 1) {
					sum += normals2[z][x + 1] * FALLOUT_RATIO;
				}
				if (z > 0) {
					sum += normals2[z - 1][x] * FALLOUT_RATIO;
				}
				if (z < l - 1) {
					sum += normals2[z + 1][x] * FALLOUT_RATIO;
				}

				if (sum.magnitude() == 0) {
					sum = Vec3f(0.0f, 1.0f, 0.0f);
				}
				normals[z][x] = sum;
			}
		}

		for (int i = 0; i < l; i++) {
			delete[] normals2[i];
		}
		delete[] normals2;

		computedNormals = true;
	}

	//Returns the normal at (x, z)
	Vec3f getNormal(int x, int z) {
		if (!computedNormals) {
			computeNormals();
		}
		return normals[z][x];
	}
};
//end class


void initRendering() {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);
	glShadeModel(GL_SMOOTH);
}

//Loads a terrain from a heightmap.  The heights of the terrain range from
//-height / 2 to height / 2.
//load terain di procedure inisialisasi
Terrain* loadTerrain(const char* filename, float height) {
	Image* image = loadBMP(filename);
	Terrain* t = new Terrain(image->width, image->height);
	for (int y = 0; y < image->height; y++) {
		for (int x = 0; x < image->width; x++) {
			unsigned char color = (unsigned char) image->pixels[3 * (y
					* image->width + x)];
			float h = height * ((color / 255.0f) - 0.5f);
			t->setHeight(x, y, h);
		}
	}

	delete image;
	t->computeNormals();
	return t;
}

float _angle = 60.0f;
//buat tipe data terain
Terrain* _terrain;
Terrain* _terrainTanah;
Terrain* _terrainAir;

const GLfloat light_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
const GLfloat light_diffuse[] = { 0.7f, 0.7f, 0.7f, 1.0f };
const GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat light_position[] = { 1.0f, 1.0f, 1.0f, 1.0f };

const GLfloat light_ambient2[] = { 0.3f, 0.3f, 0.3f, 0.0f };
const GLfloat light_diffuse2[] = { 0.3f, 0.3f, 0.3f, 0.0f };

const GLfloat mat_ambient[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat high_shininess[] = { 100.0f };

void cleanup() {
	delete _terrain;
	delete _terrainTanah;
}

//untuk di display
void drawSceneTanah(Terrain *terrain, GLfloat r, GLfloat g, GLfloat b) {
	//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	/*
	 glMatrixMode(GL_MODELVIEW);
	 glLoadIdentity();
	 glTranslatef(0.0f, 0.0f, -10.0f);
	 glRotatef(30.0f, 1.0f, 0.0f, 0.0f);
	 glRotatef(-_angle, 0.0f, 1.0f, 0.0f);

	 GLfloat ambientColor[] = {0.4f, 0.4f, 0.4f, 1.0f};
	 glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientColor);

	 GLfloat lightColor0[] = {0.6f, 0.6f, 0.6f, 1.0f};
	 GLfloat lightPos0[] = {-0.5f, 0.8f, 0.1f, 0.0f};
	 glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor0);
	 glLightfv(GL_LIGHT0, GL_POSITION, lightPos0);
	 */
	float scale = 500.0f / max(terrain->width() - 1, terrain->length() - 1);
	glScalef(scale, scale, scale);
	glTranslatef(-(float) (terrain->width() - 1) / 2, 0.0f,
			-(float) (terrain->length() - 1) / 2);

	glColor3f(r, g, b);
	for (int z = 0; z < terrain->length() - 1; z++) {
		//Makes OpenGL draw a triangle at every three consecutive vertices
		glBegin(GL_TRIANGLE_STRIP);
		for (int x = 0; x < terrain->width(); x++) {
			Vec3f normal = terrain->getNormal(x, z);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z), z);
			normal = terrain->getNormal(x, z + 1);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z + 1), z + 1);
		}
		glEnd();
	}

}




void awan(void){
glPushMatrix();
glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
glColor3ub(153, 223, 255);
glutSolidSphere(10, 50, 50);
glPopMatrix();
glPushMatrix();
glTranslatef(10,0,1);
glutSolidSphere(5, 50, 50);
glPopMatrix();
glPushMatrix();
glTranslatef(-2,6,-2);
glutSolidSphere(7, 50, 50);
glPopMatrix();
glPushMatrix();
glTranslatef(-10,-3,0);
glutSolidSphere(7, 50, 50);
glPopMatrix();
glPushMatrix();
glTranslatef(6,-2,2);
glutSolidSphere(7, 50, 50);
glPopMatrix();
}

void pagar()
{
    //tihang1
    glPushMatrix();
    glTranslated(0,0,0);
    glColor3f(0,0,0);
    glScaled(2,20,2);
    glutSolidCube(0.5f);
    glPopMatrix();



//atas
    glPushMatrix();
    glTranslated(10,0,0);
    glRotated(90,0,0,1);
    glColor3f(0,0,0);
    glScaled(2,40,2);
    glutSolidCube(0.5f);
    glPopMatrix();

//bawah
    glPushMatrix();
    glTranslated(10,4,0);
    glRotated(90,0,0,1);
    glColor3f(0,0,0);
    glScaled(2,40,2);
    glutSolidCube(0.5f);
    glPopMatrix();


}

 void monas()
 {

GLUquadricObj *quad = gluNewQuadric();
//kotak 1
     glPushMatrix();
     glTranslated(0,57,0);
     glScaled(7,1,7);
     glutSolidCube(1);
     glPopMatrix();

//kotak 2
     glPushMatrix();
     glTranslated(0,56,0);
     glScaled(10,1,10);
     glutSolidCube(1);
     glPopMatrix();

//silinder
     glPushMatrix();
     glTranslated(0.0,0.0,0.0);
     glRotated(90,-1.0,0.0,0.0);
     glRotated(135,0,0,1);
     glScaled(10,10,7);
     gluCylinder(quad,0.6,0.3,8,4,50);
     glPopMatrix();

//silinder bawah
     glPushMatrix();
     glColor3f(1.0,1.0,1.0);
     glTranslated(0.0,0.0,0.0);
     glRotated(270,-1.0,0.0,0.0);
     glRotated(135,0,0,1);
     glScaled(30,30,4);
     gluCylinder(quad,0.7,0.3,2,4,50);
     glPopMatrix();

//tutup kotak
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(0.0,-0.5,0.0);
    glRotated(270,-1.0,0.0,0.0);
    glRotated(135,0,0,1);
    glScaled(10,10,10);
    gluDisk(quad,0,2,4,1);
    glPopMatrix();

//emas
    glPushMatrix();
    glColor3f(0.8,0.498039,0.196078);
    glTranslated(0.0,57,0.0);
    glScaled(2,5,1.7);
    glutSolidSphere(1,60,360);
    glPopMatrix();

//tangga 1
    glPushMatrix();
    glTranslated(0,-8,0);
    glScaled(25,1.5,25);
    glutSolidCube(1);
    glPopMatrix();

//tangga 2
    glPushMatrix();
    glTranslated(0,-9,0);
    glScaled(29,1.5,29);
    glutSolidCube(1);
    glPopMatrix();

//tangga 3
    glPushMatrix();
    glTranslated(0,-10,0);
    glScaled(33,1.5,33);
    glutSolidCube(1);
    glPopMatrix();

//tangga 4
    glPushMatrix();
    glTranslated(0,-11,0);
    glScaled(37,1.5,37);
    glutSolidCube(1);
    glPopMatrix();

//tangga 5
    glPushMatrix();
    glTranslated(0,-12,0);
    glScaled(41,1.5,41);
    glutSolidCube(1);
    glPopMatrix();

 }

 void gedung()

 {

    GLUquadricObj *quad = gluNewQuadric();

//tihang1
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(-20,5,50);
    glRotated(90,-1.0,0.0,0.0);
    glScaled(5,5,12);
    gluCylinder(quad,0.3,0.3,2,50,50);
    glPopMatrix();

//tihang2
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(-10,5,50);
    glRotated(90,-1.0,0.0,0.0);
    glScaled(5,5,12);
    gluCylinder(quad,0.3,0.3,2,50,50);
    glPopMatrix();

//tihang3
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(0,5,50);
    glRotated(90,-1.0,0.0,0.0);
    glScaled(5,5,12);
    gluCylinder(quad,0.3,0.3,2,50,50);
    glPopMatrix();

//tihang4
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(10,5,50);
    glRotated(90,-1.0,0.0,0.0);
    glScaled(5,5,12);
    gluCylinder(quad,0.3,0.3,2,50,50);
    glPopMatrix();

//tihang5
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(20,5,50);
    glRotated(90,-1.0,0.0,0.0);
    glScaled(5,5,12);
    gluCylinder(quad,0.3,0.3,2,50,50);
    glPopMatrix();

//tihang pintu
    glPushMatrix();
    glColor3f(0.8,0.498039,0.196078);
    glTranslated(5,5,18);
    glRotated(90,-1.0,0.0,0.0);
    glScaled(3,3,8);
    gluCylinder(quad,0.1,0.1,2,50,50);
    glPopMatrix();

    glPushMatrix();
    glColor3f(0.8,0.498039,0.196078);
    glTranslated(-5,5,18);
    glRotated(90,-1.0,0.0,0.0);
    glScaled(3,3,8);
    gluCylinder(quad,0.1,0.1,2,50,50);
    glPopMatrix();

//pintu
    glPushMatrix();
    glColor3f(0.8,0.498039,0.196078);
    glTranslated(0,14,18);
    glScaled(10,15,0.5);
    glutSolidCube(1);
    glPopMatrix();

//kotak awal
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(0,20,0);
    glScaled(130,31,35);
    glutSolidCube(1);
    glPopMatrix();

//kotak kiri
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(-37.5,20,35);
    glScaled(15,30,35);
    glutSolidCube(1);
    glPopMatrix();

//kotak kanan
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(37.5,20,35);
    glScaled(15,30,35);
    glutSolidCube(1);
    glPopMatrix();

//tangga 1
    glPushMatrix();
    glTranslated(0,5.5,22.5);
    glScaled(90,1,60);
    glutSolidCube(1);
    glPopMatrix();

//tangga 2
    glPushMatrix();
    glTranslated(0,6,22.5);
    glScaled(90,1,55);
    glutSolidCube(1);
    glPopMatrix();

//tangga 3
    glPushMatrix();
    glTranslated(0,7,22.5);
    glScaled(90,1,50);
    glutSolidCube(1);
    glPopMatrix();

//tangga 4
    glPushMatrix();
    glTranslated(0,8,22.5);
    glScaled(90,1,45);
    glutSolidCube(1);
    glPopMatrix();

//tutup gedung
    glPushMatrix();
    glTranslated(0,30,6);
    glScaled(130,1.5,30);
    glutSolidCube(1);
    glPopMatrix();

//tutup gedung depan bawah
    glPushMatrix();
    glTranslated(0,30,25);
    glScaled(95,1.5,60);
    glutSolidCube(1);
    glPopMatrix();

//tutup gedung atas
    glPushMatrix();
    glTranslated(0,35,6);
    glScaled(130,1.5,30);
    glutSolidCube(1);
    glPopMatrix();

//tutup gedung depan atas
    glPushMatrix();
    glTranslated(0,35,25);
    glScaled(95,1.5,60);
    glutSolidCube(1);
    glPopMatrix();

//tutup antar atap
    glPushMatrix();
    glTranslated(0,32,47.5);
    glScaled(60,5,10);
    glutSolidCube(1);
    glPopMatrix();

//menara
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(0,30,40);
    glRotated(90,-1.0,0.0,0.0);
    glScaled(2,2,10);
    gluCylinder(quad,0.2,0.2,2,50,50);
    glPopMatrix();

//tutup menara
    glDisable(GL_CULL_FACE);
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(0,50,40);
    glRotated(90,-1,0,0);
    gluDisk(quad,0,0.5,50,1);
    glPopMatrix();
    glEnable(GL_CULL_FACE);

//tihang bendera
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(0,2,65);
    glRotated(90,-1.0,0.0,0.0);
    glScaled(2.5,2.5,20);
    gluCylinder(quad,0.2,0.2,2,50,50);
    glPopMatrix();

//dasar tihang
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(0,0,65);
    glRotated(90,-1.0,0.0,0.0);
    glScaled(10,10,1);
    gluCylinder(quad,0.2,0.2,2,50,50);
    glPopMatrix();

//tutup dasartiang
    glDisable(GL_CULL_FACE);
    glPushMatrix();
    glColor3f(0.8f, 0.4f, 0.1f);
    glTranslated(0,2,65);
    glRotated(90,-1,0,0);
    gluDisk(quad,0,2,50,1);
    glPopMatrix();
    glEnable(GL_CULL_FACE);

//benderamerah
    glPushMatrix();
    glColor3f(255,0,0);
    glTranslated(4,40,65);
    glScaled(8,2,0.1);
    glutSolidCube(1);
    glPopMatrix();

//benderaputih
    glPushMatrix();
    glColor3f(255,255,255);
    glTranslated(4,38,65);
    glScaled(8,2,0.1);
    glutSolidCube(1);
    glPopMatrix();

//tutup tihang
    glDisable(GL_CULL_FACE);
    glPushMatrix();
    glColor3f(0.8f, 0.4f, 0.1f);
    glTranslated(0,42,65);
    glRotated(90,-1,0,0);
    gluDisk(quad,0,0.5,50,1);
    glPopMatrix();
    glEnable(GL_CULL_FACE);

 }



 void pohon()
{
    GLUquadricObj *quad = gluNewQuadric();
//Batang Cemara
    glPushMatrix();
    glColor3f(0.8f, 0.4f, 0.1f);
    glTranslated(0,-3,0);
    glScaled(1.5,2,1.5);
    glRotated(90,-1.0,0.0,0.0);
    gluCylinder(quad,0.2,0.2,2,50,50);
    glPopMatrix();

//Daun Bawah
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(0.0, 1, 0.0);
    glRotatef(230, 1.5, 2, 2);
    glScaled(2,2,3);
    glutSolidCone(1.6,1,20,30);
    glPopMatrix();

//Daun Tengah
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(0.0f, 2.5, 0.0f);
    glRotatef(230, 1.5, 2, 2);
    glScaled(2,2,3);
    glutSolidCone(1.3,1,20,30);
    glPopMatrix();

//Daun Atas
    glPushMatrix();
    glColor3f(0.5f, 0.7f, 0.1f);
    glTranslatef(0,4,0);
    glRotatef(230, 1.5, 2, 2);
    glScaled(2,2,3);
    glutSolidCone(1.0,1,20,30);
    glPopMatrix();
}

void tiangistiqlal()
{
//kotak awal
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(0,10,0);
    glScaled(3,25,5);
    glutSolidCube(1);
    glPopMatrix();

}

void tiangistiqlal2()
{
//kotak awal
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(-3,10,-20);
    glScaled(5,25,3);
    glutSolidCube(1);
    glPopMatrix();
}

void istiqlal()
{
    GLUquadricObj *quad = gluNewQuadric();
//kotak besar
    glPushMatrix();
    glColor3f(0.0,0.0,0.0);
    glTranslated(0,0,-15);
    glScaled(30,30,20);
    glutSolidCube(1);
    glPopMatrix();

//kotak bawah
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(0,12,-15);
    glScaled(50,1,40);
    glutSolidCube(1);
    glPopMatrix();

//kotak tengah
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(0,15,-16);
    glScaled(40,3,28);
    glutSolidCube(1);
    glPopMatrix();

//kotak atas
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(0,18,-16);
    glScaled(45,1,38);
    glutSolidCube(1);
    glPopMatrix();

//kubah
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(0.0,19,-18);
    glScaled(10,20,10);
    glutSolidSphere(1,60,360);
    glPopMatrix();

//kotak depann
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(-10,-5,8);
    glScaled(7,1,15);
    glutSolidCube(1);
    glPopMatrix();

//kotak depann kanan
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(19,-5,15);
    glScaled(70,1,5);
    glutSolidCube(1);
    glPopMatrix();

//kotak depann
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(28,-5,-10);
    glScaled(7,1,45);
    glutSolidCube(1);
    glPopMatrix();

    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(45,-5,-10);
    glScaled(7,1,45);
    glutSolidCube(1);
    glPopMatrix();

    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(28,-10,-10);
    glScaled(4,1,5);
    glutSolidCube(1);
    glPopMatrix();

//kotak depann kanan
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(28,-5,-30);
    glScaled(30,1,5);
    glutSolidCube(1);
    glPopMatrix();

//menara kecil
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(48,-5,15);
    glRotated(90,-1,0,0);
    gluCylinder(quad,2,2,50,50,50);
    glPopMatrix();

//kubah menara
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(48,39,15);
    glRotated(90,-1,0,0);
    gluCylinder(quad,2.5,2.5,4,50,50);
    glPopMatrix();

//kubah menara2
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(48,45,15);
    glRotated(90,-1,0,0);
    gluCylinder(quad,2.5,2.5,2,50,50);
    glPopMatrix();

//kubah menara5
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(28,17,-25);
    glRotated(90,-1,0,0);
    gluCylinder(quad,1.5,1.5,4,50,50);
    glPopMatrix();

//kubah menara6
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(28,19,-25);
    glRotated(90,-1,0,0);
    gluCylinder(quad,2,2,1,50,50);
    glPopMatrix();

//tutup kubah menara5
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(28,21,-25);
    glRotated(90,-1,0,0);
    gluDisk(quad,0,1.5,50,1);
    glPopMatrix();

//kubah3
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(28,21,-25);
    glScaled(0.4,7,0.4);
    glutSolidSphere(1,60,360);
    glPopMatrix();

//tutup kubah menara6
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(28,20,-25);
    glRotated(90,-1,0,0);
    gluDisk(quad,0,2,50,1);
    glPopMatrix();

//tutup kubah menara
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(48,43,15);
    glRotated(90,-1,0,0);
    gluDisk(quad,0,2.5,50,1);
    glPopMatrix();

//tutup kubah menara2
    glDisable(GL_CULL_FACE);
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(48,38.9,15);
    glRotated(90,-1,0,0);
    gluDisk(quad,0,2.5,50,1);
    glPopMatrix();
    glEnable(GL_CULL_FACE);

//tutup kubah menara3
    glDisable(GL_CULL_FACE);
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(48,45,15);
    glRotated(90,-1,0,0);
    gluDisk(quad,0,2.5,50,1);
    glPopMatrix();
    glEnable(GL_CULL_FACE);

//tutup kubah menara4
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(48,47,15);
    glRotated(90,-1,0,0);
    gluDisk(quad,0,2.5,50,1);
    glPopMatrix();

//kubah2
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(48,50,15);
    glScaled(0.7,15,0.7);
    glutSolidSphere(1,60,360);
    glPopMatrix();

//menara gede
    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(48,-5,15);
    glRotated(90,-1,0,0);
    gluDisk(quad,0,2,50,1);
    glPopMatrix();

    glPushMatrix();
    glColor3f(1.0,1.0,1.0);
    glTranslated(48,45,15);
    glRotated(90,-1,0,0);
    gluDisk(quad,0,2,50,1);
    glPopMatrix();

}

void istiqlalfix()
{
    //kotak depan
for (int i=-60;i<40 ;i+=5){

     if (i<-45)
     {
    glPushMatrix();
    glTranslatef(i,20,-33);
    glScalef(1,1,6);
    tiangistiqlal();
    glPopMatrix();


     }
 else if (i<35)
    {
    glPushMatrix();
    glTranslatef(i,20,-45);
    glScalef(1,1,3);

    tiangistiqlal();
    glPopMatrix();
    }
    else{
    glPushMatrix();
    glTranslatef(i,20,-33);
    glScalef(1,1,6);
    tiangistiqlal();
    glPopMatrix();

    }

}
//kotak kecil kanan
for (int i=40;i>18 ;i-=4){

    glPushMatrix();
    glTranslatef(40,i,-26);
    glScalef(3,0.10,3);
    tiangistiqlal();
    glPopMatrix();

}

//kotak kecil kiri
for (int i=40;i>18 ;i-=4){

    glPushMatrix();
    glTranslatef(-55,i,-26);
    glScalef(3,0.10,3);
    tiangistiqlal();
    glPopMatrix();
}

//kotak belakang

for (int i=-45;i<64 ;i+=5){
    glPushMatrix();
    glTranslatef(i,20,-120);
    glScalef(1,1,3);
    tiangistiqlal();
    glPopMatrix();

}
//kotak kiri

for (int i=-100;i<-15;i+=5){
    glPushMatrix();
    glTranslatef(-50,20,i);
    glScalef(2,1,1);
    tiangistiqlal2();
    glPopMatrix();
}

//kotak kanan
for (int i=-100;i<5;i+=5){
    glPushMatrix();
    glTranslatef(50,20,i);
    glScalef(1,1,1);
    tiangistiqlal2();
    glPopMatrix();
}
//kotak pagar
for (int i=-37;i<136 ;i+=3){
    glPushMatrix();
    glTranslatef(i,15,10);
    glScalef(0.5,0.45,2);
    tiangistiqlal();
    glPopMatrix();
}

//kotak belakang tiang

for (int i=65;i<80 ;i+=5){

    glPushMatrix();
    glTranslatef(i,26,-120);
    glScalef(0.5,0.9,1);
    tiangistiqlal();
    glPopMatrix();
}
for (int i=65;i<80 ;i+=5){
    glPushMatrix();
    glTranslatef(i,26,-110);
    glScalef(0.5,0.9,1);
    tiangistiqlal();
    glPopMatrix();
}

for (int i=65;i<80 ;i+=5){

    glPushMatrix();
    glTranslatef(i,26,-100);
    glScalef(0.5,0.9,1);
    tiangistiqlal();
    glPopMatrix();

//kotak kecil kanan
for (int i=45;i>20 ;i-=4){

    glPushMatrix();
    glTranslatef(70,i,-110);
    glScalef(4,0.1,5);
    tiangistiqlal();
    glPopMatrix();
}
//kotak pagar belakang
for (int i=60;i<120 ;i+=3){

    glPushMatrix();
    glTranslatef(i,15,-123);
    glScalef(0.5,0.45,2);
    tiangistiqlal();
    glPopMatrix();
}
}
    glPushMatrix();
    glTranslatef(0,30,-35);
    glScalef(2.5,1,3);
    istiqlal();
    glPopMatrix();
}


unsigned int LoadTextureFromBmpFile(char *filename);

void display(void) {
	glClearStencil(0); //clear the stencil buffer
	glClearDepth(1.0f);
	glClearColor(0.0, 0.6, 0.8, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); //clear the buffers
	glLoadIdentity();
	gluLookAt(viewx, viewy, viewz, 0.0, 0.0, 5.0, 0.0, 1.0, 0.0);

	glPushMatrix();

	//glBindTexture(GL_TEXTURE_3D, texture[0]);
	drawSceneTanah(_terrain, 0.3f, 0.9f, 0.0f);
	glPopMatrix();

	//glPushMatrix();

	//glBindTexture(GL_TEXTURE_3D, texture[0]);
	//drawSceneTanah(_terrainTanah, 0.7f, 0.2f, 0.1f);
	//glPopMatrix();

	glPushMatrix();


	//glBindTexture(GL_TEXTURE_3D, texture[0]);
	drawSceneTanah(_terrainAir, 0.4902f, 0.4683f,0.4594f);
	glPopMatrix();

    glPushMatrix();
    glTranslated(0,160,0);
    awan();
    glPopMatrix();

    glPushMatrix();
    glTranslated(0,160,-90);
    awan();
    glPopMatrix();


//istiqlal
    glPushMatrix();
    glScaled(0.6,0.8,0.6);
    glTranslated(0.0,3,180);
    glRotated(180,0,1,0);
    istiqlalfix();
    glPopMatrix();

//monas
    glDisable(GL_CULL_FACE);//monas
    glPushMatrix();
    glTranslated(-2,31,-2);
    glScalef(1.5,1.5,1.5);
    monas();
    glPopMatrix();

//gedung
    glPushMatrix();
    glTranslated(0,12,-165);
    glScalef(1,1,1);
    gedung();
    glPopMatrix();

//pagar kanan 1
for (int i=44; i<=230; i+=20)
    {
    glPushMatrix();
    glTranslated(i,18,15);
    pagar();
    glPopMatrix();
    }

    //pagar kanan 2
for (int i=44; i<=230; i+=20)
    {
    glPushMatrix();
    glTranslated(i,18,-15);
    pagar();
    glPopMatrix();
    }


    //pagar kiri 1
for (int i=-44; i>=-230; i+=-20)
    {
    glPushMatrix();
    glTranslated(i,18,15);
    glRotated(180,0,1,0);
    pagar();
    glPopMatrix();
    }

    //pagar kiri 2
for (int i=-44; i>=-230; i+=-20)
    {
    glPushMatrix();
    glTranslated(i,18,-15);
    glRotated(180,0,1,0);
    pagar();
    glPopMatrix();
    }

//tihangluar kanan1
    glPushMatrix();
    glTranslated(243,18,15);
    glColor3f(0,0,0);
    glScaled(2,20,2);
    glutSolidCube(0.5f);
    glPopMatrix();


//tihangluar kanan2
    glPushMatrix();
    glTranslated(243,18,-15);
    glColor3f(0.8f, 0.4f, 0.1f);
    glScaled(2,20,2);
    glutSolidCube(0.5f);
    glPopMatrix();

//tihangluar kiri1
    glPushMatrix();
    glTranslated(-243,18,15);
    glColor3f(0.8f, 0.4f, 0.1f);
    glScaled(2,20,2);
    glutSolidCube(0.5f);
    glPopMatrix();

//tihangluar kiri2
    glPushMatrix();
    glTranslated(-243,18,-15);
    glColor3f(0.8f, 0.4f, 0.1f);
    glScaled(2,20,2);
    glutSolidCube(0.5f);
    glPopMatrix();

//hotel
    glPushMatrix();
    glTranslated(-243,18,-15);
    glColor3f(0.8f, 0.4f, 0.1f);
    glScaled(2,20,2);
    glutSolidCube(0.5f);
    glPopMatrix();

//pohon kanan
    glPushMatrix();
    glTranslatef(45,20,-45);
    glScalef(1.5,2,1.5);
    pohon();
    glPopMatrix();

     glPushMatrix();
    glTranslatef(45,20,-25);
    glScalef(1.5,2,1.5);
    pohon();
    glPopMatrix();

//
    glPushMatrix();
    glTranslatef(45,20,45);
    glScalef(1.5,2,1.5);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-45,20,45);
    glScalef(1.5,2,1.5);
    pohon();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-45,20,-45);
    glScalef(1.5,2,1.5);
    pohon();
    glPopMatrix();

	glutSwapBuffers();
	glFlush();
	rot++;
	angle++;

}

void init(void) {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glDepthFunc(GL_LESS);
	glEnable(GL_NORMALIZE);
	glEnable(GL_COLOR_MATERIAL);
	glDepthFunc(GL_LEQUAL);
	glShadeModel(GL_SMOOTH);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glEnable(GL_CULL_FACE);

	_terrain = loadTerrain("heightmap.bmp", 20);
	//_terrainTanah = loadTerrain("heightmapTanah.bmp", 20);
	_terrainAir = loadTerrain("heightmapAir.bmp", 20);

	//binding texture

}


static void kibor(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_HOME:
		viewy+=5;
		break;
	case GLUT_KEY_END:
		viewy-=5;
		break;
	case GLUT_KEY_UP:
		viewz-=5;
		break;
	case GLUT_KEY_DOWN:
		viewz+=5;
		break;

	case GLUT_KEY_RIGHT:
		viewx+=5;
		break;
	case GLUT_KEY_LEFT:
		viewx-=5;
		break;

	case GLUT_KEY_F1: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	case GLUT_KEY_F2: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient2);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse2);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	default:
		break;
	}
}

void keyboard(unsigned char key, int x, int y) {
	if (key == 'd') {

		spin = spin - 5;
		if (spin > 360.0)
			spin = spin - 360.0;
	}
	if (key == 'a') {
		spin = spin + 5;
		if (spin > 360.0)
			spin = spin - 360.0;
	}
	if (key == 'q') {
		viewz+=5;
	}
	if (key == 'e') {
		viewz-=5;
	}
	if (key == 's') {
		viewy-=5;
	}
	if (key == 'w') {
		viewy+=5;
	}
}

void reshape(int w, int h) {
	glViewport(0, 0, (GLsizei) w, (GLsizei) h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, (GLfloat) w / (GLfloat) h, 0.1, 1000.0);
	glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char **argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_STENCIL | GLUT_DEPTH); //add a stencil buffer to the window
	glutInitWindowSize(800, 600);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("Sample Terain");
	init();

	glutDisplayFunc(display);
	glutIdleFunc(display);
	glutReshapeFunc(reshape);
	glutSpecialFunc(kibor);

	glutKeyboardFunc(keyboard);

	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);

	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, high_shininess);
	glColorMaterial(GL_FRONT, GL_DIFFUSE);

	glutMainLoop();
	return 0;
}
