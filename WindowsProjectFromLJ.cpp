#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <wchar.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <math.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define MAX_VEHICLES 100
#define TIME_MULTIPLIER 180
#define IDC_MAIN_LIST 1001
#define IDC_ADD_BUTTON 1002
#define IDC_REMOVE_BUTTON 1003
#define IDC_QUERY_BUTTON 1004
#define IDC_EDIT_BUTTON 1005
#define IDC_STATS_BUTTON 1006
#define IDC_SAVE_BUTTON 1007
#define IDC_LOAD_BUTTON 1008
#define IDC_PLATE_EDIT 1009
#define IDC_TYPE_COMBO 1010
#define IDC_STATUS_TEXT 1011
#define IDC_CAPACITY_EDIT 1012
#define IDC_INIT_BUTTON 1013
#define IDC_3D_VIEW 1014
#define IDC_TIME_SLIDER 1015
#define IDC_SPEED_SLIDER 1016
#define IDC_SPEED_TEXT 1017
#define IDC_TIME_TEXT 1018
#define IDC_HELP_BUTTON 1019
#define IDC_ENV_DISPLAY 1020
#define IDC_HELP_WINDOW 2000
#define IDC_HELP_CATEGORY_BUTTON_START 2100
#define IDC_HELP_BACK_BUTTON 2200
#define IDC_HELP_TEXT 2201
#define IDC_QUERY_TYPE_COMBO 1021
#define IDC_QUERY_VALUE_EDIT 1022
#define IDC_EDIT_PLATE_EDIT 1023
#define IDC_EDIT_TYPE_COMBO 1024
#define IDC_EDIT_OK_BUTTON 1025
#define IDC_EDIT_CANCEL_BUTTON 1026
#define IDC_RESET_BUTTON 1027

typedef enum { Empty, Full } CarPortState;
typedef enum { CAR_SMALL, CAR_TRUCK_SMALL, CAR_TRUCK_MEDIUM, CAR_TRUCK_LARGE } VehicleType;

typedef struct {
    wchar_t id[32];
    CarPortState state;
    float x, z;
    float width, length;
    bool selected;
} CarPort;

typedef struct {
    wchar_t plate_number[32];
    VehicleType type;
    time_t arrival_time;
    time_t departure_time;
    float primary_r, primary_g, primary_b;
    float secondary_r, secondary_g, secondary_b;
    float accent_r, accent_g, accent_b;
    int parking_position;
    bool on_waiting_queue;
} VehicleInfo;

typedef struct {
    CarPort* car_ports;
    VehicleInfo** vehicles_in_lot;
    VehicleInfo** waiting_queue;
    float unit_price;
    int capacity;
    int current_count;
    int waiting_count;
    int total_vehicles;
    int portsPerRow;
    int portsPerCol;
    float layoutWidth;
    float layoutDepth;
} ParkingLot;

ParkingLot parkingLot = { 0 };
HWND g_hMainWnd = NULL;
HWND g_hListView = NULL;
HWND g_hPlateEdit = NULL;
HWND g_hTypeCombo = NULL;
HWND g_hStatusText = NULL;
HWND g_hCapacityEdit = NULL;
HWND g_h3DView = NULL;
HWND g_hTimeSlider = NULL;
HWND g_hSpeedSlider = NULL;
HWND g_hSpeedText = NULL;
HWND g_hTimeText = NULL;
HWND g_hHelpWindow = NULL;
HWND g_hQueryTypeCombo = NULL;
HWND g_hQueryValueEdit = NULL;
HWND g_hEditWindow = NULL;
HFONT g_hFont = NULL;
HFONT g_hTitleFont = NULL;
HFONT g_hHelpTitleFont = NULL;
HFONT g_hHelpContentFont = NULL;
HDC g_hDC = NULL;
HGLRC g_hRC = NULL;

GLuint parkingLotDisplayList = 0;
GLuint carDisplayList[4] = { 0 };
bool needRedraw = true;
DWORD lastRenderTime = 0;
bool enableEnvironment = true;

float cameraPosX = 0.0f;
float cameraPosY = 40.0f;
float cameraPosZ = 60.0f;
float cameraYaw = -90.0f;
float cameraPitch = -30.0f;
bool rightMousePressed = false;
bool leftMousePressed = false;
int lastMouseX = 0, lastMouseY = 0;
int selectedCarPort = -1;

struct { bool w, a, s, d, space, ctrl, r, g, e, t, y; } keys = { false };

float timeOfDay = 10.0f;
float timeSpeed = 3600.0f;
bool isDayTime = true;
bool sunVisible = false;
bool moonVisible = false;
float sunAngle = 0.0f;
float moonAngle = 0.0f;

typedef struct {
    float x, y, z;
    float brightness;
    float twinkleSpeed;
    float twinklePhase;
} Star;

Star stars[1500];

typedef struct {
    float x, y, z;
    float size;
    float speed;
    float rotation;
    float alpha;
} Cloud;

Cloud clouds[100];

typedef struct {
    float x, y, z;
    float size;
    float rotation;
    int type;
    float swayAngle;
    float swaySpeed;
} Decoration;

Decoration decorations[50];

typedef struct {
    float x, y, z;
    float size;
    float speed;
    float angle;
    float altitude;
} Bird;

Bird birds[20];

typedef struct {
    float x, y, z;
    float size;
    float rotation;
    float height;
    float trunkHeight;
    int leafType;
} Tree;

Tree trees[30];

typedef struct {
    float x, y, z;
    float height;
    float topSize;
    bool isOn;
} StreetLight;

StreetLight streetLights[12];

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK View3DWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK HelpWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EditWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void InitializeParkingLot(int capacity);
void CleanupParkingLot();
void AddVehicleToListView(VehicleInfo* vehicle, int position, bool isWaiting);
void UpdateListView();
void AddVehicle(const wchar_t* plate, VehicleType type);
bool RemoveVehicle(const wchar_t* plate);
void QueryVehicle(int queryType, const wchar_t* queryValue);
void EditVehicle(const wchar_t* plate);
void ShowStatistics();
void SaveToFile();
void LoadFromFile();
void SetStatusText(const wchar_t* text);
const wchar_t* GetVehicleTypeName(VehicleType type);
void Setup3DView(HWND hWnd);
void Render3DView();
void DrawParkingLot3D();
void UpdateParkingLotDisplayList();
void CreateVehicleDisplayLists();
void DrawVehicle3D(float x, float y, float z, VehicleType type, float r1, float g1, float b1, float r2, float g2, float b2, float r3, float g3, float b3);
void DrawSolidCube(float size);
void DrawWheel(float x, float y, float z, float radius, float width, float r, float g, float b);
void DrawCar(float length, float width, float height, float r1, float g1, float b1, float r2, float g2, float b2, float r3, float g3, float b3);
void DrawTruck(float length, float width, float height, float r1, float g1, float b1, float r2, float g2, float b2, float r3, float g3, float b3, VehicleType type);
void ProcessCameraInput();
void ResetCamera();
int GetCarPortAtMousePosition(int mouseX, int mouseY);
void ShowCarPortInfo(int carPortIndex);
void UpdateTimeSystem();
void DrawSun();
void DrawMoon();
void DrawStars();
void DrawClouds();
void SetupLighting();
void SetTimeOfDay(float hour);
void InitializeStars();
void InitializeClouds();
void InitializeDecorations();
void UpdateSpeedText();
void UpdateTimeText();
void ToggleEnvironment();
void DrawDetailedTree(float x, float y, float z, float size, int leafType);
void DrawDetailedStreetLight(float x, float y, float z, float size, bool isOn);
void DrawBench(float x, float y, float z, float size);
void DrawDecorations();
bool IsPositionInParkingLot(float x, float z);
void DrawBirds();
void InitializeBirds();
void DrawGround();
void InitializeTrees();
void InitializeStreetLights();
void DrawTrees();
void DrawStreetLights();
void DrawBirdModel(float x, float y, float z, float size, float rotation);
void GenerateVehicleColors(VehicleType type, float* r1, float* g1, float* b1, float* r2, float* g2, float* b2, float* r3, float* g3, float* b3);
void ShowHelpWindow();
void ShowHelpContent(int category);
void ToggleTimeSpeed(bool increase);
void UpdateVehicleInfo(VehicleInfo* oldVehicle, const wchar_t* newPlate, VehicleType newType);
bool CompareVehicleInfo(const VehicleInfo* vehicle, int queryType, const wchar_t* queryValue);
int FindVehicleByPlate(const wchar_t* plate);
int FindEmptyParkingSlot();
void ResetProgram();

void DrawSolidCube(float size) {
    float s = size * 0.5f;
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-s, -s, s);
    glVertex3f(s, -s, s);
    glVertex3f(s, s, s);
    glVertex3f(-s, s, s);
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(-s, -s, -s);
    glVertex3f(-s, s, -s);
    glVertex3f(s, s, -s);
    glVertex3f(s, -s, -s);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-s, s, -s);
    glVertex3f(-s, s, s);
    glVertex3f(s, s, s);
    glVertex3f(s, s, -s);
    glNormal3f(0.0f, -1.f, 0.0f);
    glVertex3f(-s, -s, -s);
    glVertex3f(s, -s, -s);
    glVertex3f(s, -s, s);
    glVertex3f(-s, -s, s);
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-s, -s, -s);
    glVertex3f(-s, -s, s);
    glVertex3f(-s, s, s);
    glVertex3f(-s, s, -s);
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(s, -s, -s);
    glVertex3f(s, s, -s);
    glVertex3f(s, s, s);
    glVertex3f(s, -s, s);
    glEnd();
}

void DrawSphere(float radius, int slices, int stacks) {
    static GLUquadric* quadric = NULL;
    if (!quadric) quadric = gluNewQuadric();
    gluSphere(quadric, radius, slices, stacks);
}

void DrawCylinder(float radius, float height, int slices) {
    static GLUquadric* quadric = NULL;
    if (!quadric) quadric = gluNewQuadric();
    gluCylinder(quadric, radius, radius, height, slices, 1);
}

void DrawDisk(float radius, int slices) {
    static GLUquadric* quadric = NULL;
    if (!quadric) quadric = gluNewQuadric();
    gluDisk(quadric, 0.0f, radius, slices, 1);
}

void GenerateVehicleColors(VehicleType type, float* r1, float* g1, float* b1, float* r2, float* g2, float* b2, float* r3, float* g3, float* b3) {
    static struct ColorScheme {
        float r1, g1, b1, r2, g2, b2, r3, g3, b3;
    } schemes[] = {
        {0.95,0.92,0.85, 0.85,0.82,0.75, 0.70,0.15,0.15},
        {0.25,0.85,0.95, 0.15,0.75,0.85, 0.98,0.98,0.98},
        {0.15,0.95,0.35, 0.05,0.85,0.25, 0.95,0.95,0.30},
        {0.95,0.65,0.25, 0.85,0.55,0.15, 0.30,0.30,0.85},
        {0.85,0.25,0.95, 0.75,0.15,0.85, 0.95,0.95,0.40},
        {0.95,0.40,0.70, 0.85,0.30,0.60, 0.20,0.80,0.90},
        {0.25,0.95,0.85, 0.15,0.85,0.75, 0.95,0.40,0.20},
        {0.95,0.90,0.25, 0.85,0.80,0.15, 0.30,0.30,0.95},
        {0.70,0.25,0.15, 0.60,0.15,0.05, 0.90,0.85,0.80},
        {0.20,0.20,0.95, 0.10,0.10,0.85, 0.95,0.95,0.25},
        {0.95,0.30,0.25, 0.85,0.20,0.15, 0.25,0.95,0.95},
        {0.30,0.95,0.30, 0.20,0.85,0.20, 0.95,0.30,0.95}
    };

    int schemeIndex = rand() % 12;
    int colorVariant = rand() % 3;

    switch (colorVariant) {
    case 0:
        *r1 = schemes[schemeIndex].r1;
        *g1 = schemes[schemeIndex].g1;
        *b1 = schemes[schemeIndex].b1;
        *r2 = schemes[schemeIndex].r2;
        *g2 = schemes[schemeIndex].g2;
        *b2 = schemes[schemeIndex].b2;
        *r3 = schemes[schemeIndex].r3;
        *g3 = schemes[schemeIndex].g3;
        *b3 = schemes[schemeIndex].b3;
        break;
    case 1:
        *r1 = schemes[schemeIndex].r2;
        *g1 = schemes[schemeIndex].g2;
        *b1 = schemes[schemeIndex].b2;
        *r2 = schemes[schemeIndex].r3;
        *g2 = schemes[schemeIndex].g3;
        *b2 = schemes[schemeIndex].b3;
        *r3 = schemes[schemeIndex].r1;
        *g3 = schemes[schemeIndex].g1;
        *b3 = schemes[schemeIndex].b1;
        break;
    case 2:
        *r1 = schemes[schemeIndex].r3;
        *g1 = schemes[schemeIndex].g3;
        *b1 = schemes[schemeIndex].b3;
        *r2 = schemes[schemeIndex].r1;
        *g2 = schemes[schemeIndex].g1;
        *b2 = schemes[schemeIndex].b1;
        *r3 = schemes[schemeIndex].r2;
        *g3 = schemes[schemeIndex].g2;
        *b3 = schemes[schemeIndex].b2;
        break;
    }

    if (type == CAR_TRUCK_LARGE) {
        *r1 *= 0.95f; *g1 *= 0.95f; *b1 *= 0.95f;
        *r2 *= 0.95f; *g2 *= 0.95f; *b2 *= 0.95f;
        *r3 *= 0.95f; *g3 *= 0.95f; *b3 *= 0.95f;
    }
    else if (type == CAR_TRUCK_MEDIUM) {
        *r1 *= 0.98f; *g1 *= 0.98f; *b1 *= 0.98f;
        *r2 *= 0.98f; *g2 *= 0.98f; *b2 *= 0.98f;
        *r3 *= 0.98f; *g3 *= 0.98f; *b3 *= 0.98f;
    }
}

void DrawWheel(float x, float y, float z, float radius, float width, float r, float g, float b) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
    glColor3f(0.25f, 0.25f, 0.25f);
    DrawCylinder(radius, width, 16);
    glColor3f(0.15f, 0.15f, 0.15f);
    glPushMatrix();
    glTranslatef(0, 0, width / 2);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    DrawDisk(radius * 0.95f, 16);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0, 0, -width / 2);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    DrawDisk(radius * 0.95f, 16);
    glPopMatrix();
    glColor3f(r * 0.8f, g * 0.8f, b * 0.8f);
    glPushMatrix();
    glTranslatef(0, 0, width / 2 - 0.05f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    DrawDisk(radius * 0.4f, 8);
    glPopMatrix();
    glPopMatrix();
}

void DrawCar(float length, float width, float height, float r1, float g1, float b1, float r2, float g2, float b2, float r3, float g3, float b3) {
    glColor3f(r1, g1, b1);
    glPushMatrix();
    glScalef(width, height * 0.4f, length * 0.85f);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glColor3f(r2, g2, b2);
    glPushMatrix();
    glTranslatef(0, height * 0.4f, 0);
    glScalef(width * 0.9f, height * 0.3f, length * 0.7f);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.1f, 0.15f, 0.2f, 0.7f);
    glBegin(GL_QUADS);
    glVertex3f(-width * 0.35f, height * 0.4f, -length * 0.35f);
    glVertex3f(width * 0.35f, height * 0.4f, -length * 0.35f);
    glVertex3f(width * 0.35f, height * 0.7f, -length * 0.15f);
    glVertex3f(-width * 0.35f, height * 0.7f, -length * 0.15f);
    glVertex3f(-width * 0.35f, height * 0.4f, length * 0.35f);
    glVertex3f(width * 0.35f, height * 0.4f, length * 0.35f);
    glVertex3f(width * 0.35f, height * 0.7f, length * 0.15f);
    glVertex3f(-width * 0.35f, height * 0.7f, length * 0.15f);
    glVertex3f(-width * 0.45f, height * 0.4f, -length * 0.25f);
    glVertex3f(-width * 0.45f, height * 0.4f, length * 0.25f);
    glVertex3f(-width * 0.45f, height * 0.7f, length * 0.15f);
    glVertex3f(-width * 0.45f, height * 0.7f, -length * 0.15f);
    glVertex3f(width * 0.45f, height * 0.4f, -length * 0.25f);
    glVertex3f(width * 0.45f, height * 0.4f, length * 0.25f);
    glVertex3f(width * 0.45f, height * 0.7f, length * 0.15f);
    glVertex3f(width * 0.45f, height * 0.7f, -length * 0.15f);
    glEnd();
    glDisable(GL_BLEND);
    glColor3f(0.2f, 0.2f, 0.2f);
    glPushMatrix();
    glTranslatef(0, height * 0.15f, -length * 0.48f);
    glScalef(width * 0.95f, height * 0.15f, 0.1f);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0, height * 0.15f, length * 0.48f);
    glScalef(width * 0.95f, height * 0.15f, 0.1f);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glColor3f(1.0f, 1.0f, 0.8f);
    glPushMatrix();
    glTranslatef(-width * 0.35f, height * 0.25f, -length * 0.48f);
    glScalef(0.12f, 0.08f, 0.05f);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(width * 0.35f, height * 0.25f, -length * 0.48f);
    glScalef(0.12f, 0.08f, 0.05f);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glColor3f(0.8f, 0.1f, 0.1f);
    glPushMatrix();
    glTranslatef(-width * 0.35f, height * 0.25f, length * 0.48f);
    glScalef(0.12f, 0.08f, 0.05f);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(width * 0.35f, height * 0.25f, length * 0.48f);
    glScalef(0.12f, 0.08f, 0.05f);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glColor3f(r1 * 0.9f, g1 * 0.9f, b1 * 0.9f);
    glPushMatrix();
    glTranslatef(-width * 0.52f, height * 0.5f, -length * 0.2f);
    glScalef(0.08f, 0.05f, 0.04f);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(width * 0.52f, height * 0.5f, -length * 0.2f);
    glScalef(0.08f, 0.05f, 0.04f);
    DrawSolidCube(1.0f);
    glPopMatrix();
}

void DrawTruck(float length, float width, float height, float r1, float g1, float b1, float r2, float g2, float b2, float r3, float g3, float b3, VehicleType type) {
    float cabLength = (type == CAR_TRUCK_SMALL) ? length * 0.35f : (type == CAR_TRUCK_MEDIUM) ? length * 0.3f : length * 0.25f;
    float cargoLength = (type == CAR_TRUCK_SMALL) ? length * 0.65f : (type == CAR_TRUCK_MEDIUM) ? length * 0.7f : length * 0.75f;
    float cabHeight = (type == CAR_TRUCK_SMALL) ? height * 0.8f : (type == CAR_TRUCK_MEDIUM) ? height * 0.9f : height * 1.0f;
    float cargoHeight = (type == CAR_TRUCK_SMALL) ? height * 0.7f : (type == CAR_TRUCK_MEDIUM) ? height * 0.8f : height * 0.9f;

    glColor3f(r1 * 0.95f, g1 * 0.95f, b1 * 0.95f);
    glPushMatrix();
    glTranslatef(0, cabHeight * 0.5f, -cargoLength * 0.5f);
    glScalef(width * 0.92f, cabHeight, cabLength);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glColor3f(r2, g2, b2);
    glPushMatrix();
    glTranslatef(0, cabHeight, -cargoLength * 0.5f);
    glScalef(width * 0.85f, height * 0.12f, cabLength * 0.9f);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.1f, 0.15f, 0.2f, 0.7f);
    glBegin(GL_QUADS);
    glVertex3f(-width * 0.35f, cabHeight * 0.5f, -cargoLength * 0.5f - cabLength * 0.4f);
    glVertex3f(width * 0.35f, cabHeight * 0.5f, -cargoLength * 0.5f - cabLength * 0.4f);
    glVertex3f(width * 0.35f, cabHeight * 0.9f, -cargoLength * 0.5f - cabLength * 0.1f);
    glVertex3f(-width * 0.35f, cabHeight * 0.9f, -cargoLength * 0.5f - cabLength * 0.1f);
    glVertex3f(-width * 0.48f, cabHeight * 0.5f, -cargoLength * 0.5f - cabLength * 0.4f);
    glVertex3f(-width * 0.48f, cabHeight * 0.5f, -cargoLength * 0.5f);
    glVertex3f(-width * 0.48f, cabHeight * 0.9f, -cargoLength * 0.5f);
    glVertex3f(-width * 0.48f, cabHeight * 0.9f, -cargoLength * 0.5f - cabLength * 0.4f);
    glVertex3f(width * 0.48f, cabHeight * 0.5f, -cargoLength * 0.5f - cabLength * 0.4f);
    glVertex3f(width * 0.48f, cabHeight * 0.5f, -cargoLength * 0.5f);
    glVertex3f(width * 0.48f, cabHeight * 0.9f, -cargoLength * 0.5f);
    glVertex3f(width * 0.48f, cabHeight * 0.9f, -cargoLength * 0.5f - cabLength * 0.4f);
    glEnd();
    glDisable(GL_BLEND);
    glColor3f(r1, g1, b1);
    glPushMatrix();
    glTranslatef(0, cargoHeight * 0.5f, cargoLength * 0.25f);
    glScalef(width * 0.98f, cargoHeight, cargoLength);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glColor3f(0.4f, 0.4f, 0.4f);
    for (int i = -1; i <= 1; i += 2) {
        for (int j = -1; j <= 1; j += 2) {
            glPushMatrix();
            glTranslatef(i * width * 0.48f, cargoHeight, j * cargoLength * 0.48f);
            glScalef(0.05f, 0.05f, 0.05f);
            DrawSolidCube(1.0f);
            glPopMatrix();
        }
    }
    glColor3f(0.25f, 0.25f, 0.25f);
    glPushMatrix();
    glTranslatef(0, height * 0.15f, -cargoLength * 0.5f - cabLength * 0.49f);
    glScalef(width * 0.92f, height * 0.2f, 0.12f);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0, height * 0.15f, cargoLength * 0.5f + cargoLength * 0.49f);
    glScalef(width * 0.92f, height * 0.2f, 0.12f);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glColor3f(1.0f, 1.0f, 0.8f);
    glPushMatrix();
    glTranslatef(-width * 0.35f, height * 0.25f, -cargoLength * 0.5f - cabLength * 0.48f);
    glScalef(0.18f, 0.12f, 0.06f);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(width * 0.35f, height * 0.25f, -cargoLength * 0.5f - cabLength * 0.48f);
    glScalef(0.18f, 0.12f, 0.06f);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glColor3f(0.8f, 0.1f, 0.1f);
    glPushMatrix();
    glTranslatef(-width * 0.35f, height * 0.25f, cargoLength * 0.5f + cargoLength * 0.49f);
    glScalef(0.18f, 0.12f, 0.06f);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(width * 0.35f, height * 0.25f, cargoLength * 0.5f + cargoLength * 0.49f);
    glScalef(0.18f, 0.12f, 0.06f);
    DrawSolidCube(1.0f);
    glPopMatrix();
    if (type == CAR_TRUCK_LARGE) {
        glColor3f(0.3f, 0.3f, 0.3f);
        glPushMatrix();
        glTranslatef(-width * 0.25f, 0.2f, -cargoLength * 0.5f - cabLength * 0.3f);
        glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
        DrawCylinder(0.1f, 0.5f, 8);
        glPopMatrix();
    }
}

void CreateVehicleDisplayLists() {
    for (int i = 0; i < 4; i++) {
        if (carDisplayList[i]) continue;
        carDisplayList[i] = glGenLists(1);
        if (!carDisplayList[i]) continue;
        glNewList(carDisplayList[i], GL_COMPILE);
        float bodyLength, bodyWidth, bodyHeight;
        float wheelRadius, wheelWidth;
        float wheelYOffset, wheelXOffset;
        float r1, g1, b1, r2, g2, b2, r3, g3, b3;
        GenerateVehicleColors((VehicleType)i, &r1, &g1, &b1, &r2, &g2, &b2, &r3, &g3, &b3);
        switch (i) {
        case CAR_SMALL:
            bodyLength = 3.2f; bodyWidth = 1.4f; bodyHeight = 1.2f;
            wheelRadius = 0.25f; wheelWidth = 0.15f;
            wheelYOffset = -0.25f; wheelXOffset = bodyWidth / 2 - wheelRadius - 0.15f;
            DrawCar(bodyLength, bodyWidth, bodyHeight, r1, g1, b1, r2, g2, b2, r3, g3, b3);
            DrawWheel(-wheelXOffset, wheelYOffset, -bodyLength / 2 + wheelRadius + 0.3f, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(wheelXOffset, wheelYOffset, -bodyLength / 2 + wheelRadius + 0.3f, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(-wheelXOffset, wheelYOffset, bodyLength / 2 - wheelRadius - 0.3f, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(wheelXOffset, wheelYOffset, bodyLength / 2 - wheelRadius - 0.3f, wheelRadius, wheelWidth, r3, g3, b3);
            break;
        case CAR_TRUCK_SMALL:
            bodyLength = 4.0f; bodyWidth = 1.6f; bodyHeight = 1.6f;
            wheelRadius = 0.3f; wheelWidth = 0.2f;
            wheelYOffset = -0.3f; wheelXOffset = bodyWidth / 2 - wheelRadius - 0.15f;
            DrawTruck(bodyLength, bodyWidth, bodyHeight, r1, g1, b1, r2, g2, b2, r3, g3, b3, CAR_TRUCK_SMALL);
            DrawWheel(-wheelXOffset, wheelYOffset, -bodyLength * 0.4f + wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(wheelXOffset, wheelYOffset, -bodyLength * 0.4f + wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(-wheelXOffset, wheelYOffset, bodyLength * 0.25f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(wheelXOffset, wheelYOffset, bodyLength * 0.25f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            break;
        case CAR_TRUCK_MEDIUM:
            bodyLength = 5.5f; bodyWidth = 2.0f; bodyHeight = 2.0f;
            wheelRadius = 0.35f; wheelWidth = 0.25f;
            wheelYOffset = -0.35f; wheelXOffset = bodyWidth / 2 - wheelRadius - 0.15f;
            DrawTruck(bodyLength, bodyWidth, bodyHeight, r1, g1, b1, r2, g2, b2, r3, g3, b3, CAR_TRUCK_MEDIUM);
            DrawWheel(-wheelXOffset, wheelYOffset, -bodyLength * 0.35f + wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(wheelXOffset, wheelYOffset, -bodyLength * 0.35f + wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(-wheelXOffset, wheelYOffset, bodyLength * 0.2f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(wheelXOffset, wheelYOffset, bodyLength * 0.2f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(-wheelXOffset, wheelYOffset, bodyLength * 0.4f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(wheelXOffset, wheelYOffset, bodyLength * 0.4f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            break;
        case CAR_TRUCK_LARGE:
            bodyLength = 7.0f; bodyWidth = 2.4f; bodyHeight = 2.6f;
            wheelRadius = 0.4f; wheelWidth = 0.3f;
            wheelYOffset = -0.4f; wheelXOffset = bodyWidth / 2 - wheelRadius - 0.15f;
            DrawTruck(bodyLength, bodyWidth, bodyHeight, r1, g1, b1, r2, g2, b2, r3, g3, b3, CAR_TRUCK_LARGE);
            DrawWheel(-wheelXOffset, wheelYOffset, -bodyLength * 0.3f + wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(wheelXOffset, wheelYOffset, -bodyLength * 0.3f + wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(-wheelXOffset, wheelYOffset, bodyLength * 0.15f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(wheelXOffset, wheelYOffset, bodyLength * 0.15f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(-wheelXOffset, wheelYOffset, bodyLength * 0.35f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(wheelXOffset, wheelYOffset, bodyLength * 0.35f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(-wheelXOffset, wheelYOffset, bodyLength * 0.55f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            DrawWheel(wheelXOffset, wheelYOffset, bodyLength * 0.55f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
            break;
        }
        glEndList();
    }
}

void ResetCamera() {
    if (parkingLot.capacity > 0) {
        float maxExtent = max(parkingLot.layoutWidth, parkingLot.layoutDepth);
        cameraPosX = 0.0f;
        cameraPosY = maxExtent * 0.7f + 25.0f;
        cameraPosZ = maxExtent * 1.3f;
    }
    else {
        cameraPosX = 0.0f;
        cameraPosY = 40.0f;
        cameraPosZ = 60.0f;
    }
    cameraYaw = -90.0f;
    cameraPitch = -30.0f;
    needRedraw = true;
}

void ProcessCameraInput() {
    float cameraSpeed = 1.5f;
    float radYaw = cameraYaw * 3.14159f / 180.0f;
    float radPitch = cameraPitch * 3.14159f / 180.0f;
    float frontX = cos(radYaw) * cos(radPitch);
    float frontY = sin(radPitch);
    float frontZ = sin(radYaw) * cos(radPitch);
    float length = sqrt(frontX * frontX + frontY * frontY + frontZ * frontZ);
    if (length > 0) {
        frontX /= length;
        frontY /= length;
        frontZ /= length;
    }
    float rightX = frontZ;
    float rightZ = -frontX;
    float rightLength = sqrt(rightX * rightX + rightZ * rightZ);
    if (rightLength > 0) {
        rightX /= rightLength;
        rightZ /= rightLength;
    }
    if (keys.w) {
        cameraPosX += frontX * cameraSpeed;
        cameraPosY += frontY * cameraSpeed;
        cameraPosZ += frontZ * cameraSpeed;
        needRedraw = true;
    }
    if (keys.s) {
        cameraPosX -= frontX * cameraSpeed;
        cameraPosY -= frontY * cameraSpeed;
        cameraPosZ -= frontZ * cameraSpeed;
        needRedraw = true;
    }
    if (keys.a) {
        cameraPosX += rightX * cameraSpeed;
        cameraPosZ += rightZ * cameraSpeed;
        needRedraw = true;
    }
    if (keys.d) {
        cameraPosX -= rightX * cameraSpeed;
        cameraPosZ -= rightZ * cameraSpeed;
        needRedraw = true;
    }
    if (keys.space) {
        cameraPosY += cameraSpeed;
        needRedraw = true;
    }
    if (keys.ctrl) {
        cameraPosY -= cameraSpeed;
        needRedraw = true;
    }
    if (keys.r) {
        ResetCamera();
        keys.r = false;
    }
    if (keys.g) {
        ShowHelpWindow();
        keys.g = false;
    }
    if (keys.e) {
        ToggleEnvironment();
        keys.e = false;
    }
    if (keys.t) {
        ToggleTimeSpeed(true);
        keys.t = false;
    }
    if (keys.y) {
        ToggleTimeSpeed(false);
        keys.y = false;
    }
    if (cameraPosY < 5.0f) cameraPosY = 5.0f;
    if (cameraPosY > 200.0f) cameraPosY = 200.0f;
    if (cameraPitch > 89.0f) cameraPitch = 89.0f;
    if (cameraPitch < -89.0f) cameraPitch = -89.0f;
}

void Setup3DView(HWND hWnd) {
    if (!hWnd) return;
    g_hDC = GetDC(hWnd);
    if (!g_hDC) return;
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        24, 8, 0, PFD_MAIN_PLANE, 0, 0, 0, 0
    };
    int pixelFormat = ChoosePixelFormat(g_hDC, &pfd);
    if (!pixelFormat || !SetPixelFormat(g_hDC, pixelFormat, &pfd)) return;
    g_hRC = wglCreateContext(g_hDC);
    if (!g_hRC || !wglMakeCurrent(g_hDC, g_hRC)) return;
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POINT_SMOOTH);
    RECT rc;
    GetClientRect(hWnd, &rc);
    glViewport(0, 0, rc.right, rc.bottom);
}

void InitializeStars() {
    srand(12345);
    for (int i = 0; i < 1500; i++) {
        stars[i].x = (rand() % 4000) - 2000.0f;
        stars[i].y = 80.0f + (rand() % 200);
        stars[i].z = (rand() % 4000) - 2000.0f;
        if (rand() % 5 == 0) {
            stars[i].brightness = 0.7f + (rand() % 30) / 100.0f;
        }
        else {
            stars[i].brightness = 0.3f + (rand() % 50) / 100.0f;
        }
        stars[i].twinkleSpeed = 0.3f + (rand() % 150) / 100.0f;
        stars[i].twinklePhase = (rand() % 100) / 100.0f * 3.14159f * 2.0f;
    }
}

void InitializeClouds() {
    srand(54321);
    for (int i = 0; i < 100; i++) {
        clouds[i].x = (rand() % 4000) - 2000.0f;
        clouds[i].y = 20.0f + (rand() % 60);
        clouds[i].z = (rand() % 4000) - 2000.0f;
        clouds[i].size = 3.0f + (rand() % 20) / 2.0f;
        clouds[i].speed = 0.2f + (rand() % 20) / 20.0f;
        clouds[i].rotation = (rand() % 360) / 180.0f * 3.14159f;
        clouds[i].alpha = 0.4f + (rand() % 60) / 100.0f;
    }
}

void InitializeTrees() {
    srand(33333);
    float halfWidth = parkingLot.layoutWidth / 2.0f + 40.0f;
    float halfDepth = parkingLot.layoutDepth / 2.0f + 40.0f;
    for (int i = 0; i < 30; i++) {
        trees[i].size = 1.5f + (rand() % 15) / 5.0f;
        trees[i].rotation = (rand() % 360) / 180.0f * 3.14159f;
        trees[i].height = 1.0f + trees[i].size * 0.8f;
        trees[i].trunkHeight = 1.0f + trees[i].size * 0.5f;
        trees[i].leafType = rand() % 3;
        int side = rand() % 4;
        float offset = 10.0f + (rand() % 30);
        switch (side) {
        case 0:
            trees[i].x = -halfWidth - offset;
            trees[i].z = (rand() % (int)(halfDepth * 2)) - halfDepth;
            break;
        case 1:
            trees[i].x = halfWidth + offset;
            trees[i].z = (rand() % (int)(halfDepth * 2)) - halfDepth;
            break;
        case 2:
            trees[i].x = (rand() % (int)(halfWidth * 2)) - halfWidth;
            trees[i].z = -halfDepth - offset;
            break;
        case 3:
            trees[i].x = (rand() % (int)(halfWidth * 2)) - halfWidth;
            trees[i].z = halfDepth + offset;
            break;
        }
        trees[i].y = 0.0f;
    }
}

void InitializeStreetLights() {
    srand(44444);
    float halfWidth = parkingLot.layoutWidth / 2.0f + 8.0f;
    float halfDepth = parkingLot.layoutDepth / 2.0f + 8.0f;
    StreetLight lights[] = {
        {-halfWidth + 2.0f, 0.0f, -halfDepth + 15.0f, 7.0f, 0.8f, !isDayTime},
        {-halfWidth + 2.0f, 0.0f, 0.0f, 6.5f, 0.85f, !isDayTime},
        {-halfWidth + 2.0f, 0.0f, halfDepth - 15.0f, 7.0f, 0.8f, !isDayTime},
        {halfWidth - 2.0f, 0.0f, -halfDepth + 15.0f, 7.0f, 0.8f, !isDayTime},
        {halfWidth - 2.0f, 0.0f, 0.0f, 6.5f, 0.85f, !isDayTime},
        {halfWidth - 2.0f, 0.0f, halfDepth - 15.0f, 7.0f, 0.8f, !isDayTime},
        {-halfWidth + 20.0f, 0.0f, -halfDepth + 2.0f, 6.0f, 0.75f, !isDayTime},
        {0.0f, 0.0f, -halfDepth + 2.0f, 7.0f, 0.9f, !isDayTime},
        {halfWidth - 20.0f, 0.0f, -halfDepth + 2.0f, 6.0f, 0.75f, !isDayTime},
        {-halfWidth + 20.0f, 0.0f, halfDepth - 2.0f, 6.0f, 0.75f, !isDayTime},
        {0.0f, 0.0f, halfDepth - 2.0f, 7.0f, 0.9f, !isDayTime},
        {halfWidth - 20.0f, 0.0f, halfDepth - 2.0f, 6.0f, 0.75f, !isDayTime}
    };
    memcpy(streetLights, lights, sizeof(lights));
}

void InitializeBirds() {
    srand(55555);
    for (int i = 0; i < 20; i++) {
        birds[i].x = (rand() % 400) - 200.0f;
        birds[i].y = 15.0f + (rand() % 30);
        birds[i].z = (rand() % 400) - 200.0f;
        birds[i].size = 0.3f + (rand() % 10) / 10.0f;
        birds[i].speed = 0.5f + (rand() % 15) / 10.0f;
        birds[i].angle = (rand() % 360) / 180.0f * 3.14159f;
        birds[i].altitude = birds[i].y;
    }
}

void InitializeDecorations() {
    srand(98765);
    float halfWidth = parkingLot.layoutWidth / 2.0f + 25.0f;
    float halfDepth = parkingLot.layoutDepth / 2.0f + 25.0f;
    for (int i = 0; i < 50; i++) {
        decorations[i].type = rand() % 3;
        decorations[i].size = 1.0f + (rand() % 20) / 5.0f;
        decorations[i].rotation = (rand() % 360) / 180.0f * 3.14159f;
        decorations[i].swayAngle = (rand() % 100) / 100.0f * 3.14159f * 2.0f;
        decorations[i].swaySpeed = 0.5f + (rand() % 10) / 20.0f;
        int attempts = 0;
        while (attempts < 100) {
            float angle = (rand() % 360) / 180.0f * 3.14159f;
            float distance = halfWidth + 15.0f + (rand() % 30);
            decorations[i].x = cos(angle) * distance;
            decorations[i].z = sin(angle) * distance;
            if (!IsPositionInParkingLot(decorations[i].x, decorations[i].z) &&
                fabs(decorations[i].x) > halfWidth - 10.0f || fabs(decorations[i].z) > halfDepth - 10.0f) break;
            attempts++;
        }
        decorations[i].y = 0.0f;
    }
    InitializeTrees();
    InitializeStreetLights();
    InitializeBirds();
}

void DrawDetailedTree(float x, float y, float z, float size, int leafType) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glColor3f(0.35f, 0.25f, 0.15f);
    glPushMatrix();
    glScalef(0.4f * size, 2.0f * size, 0.4f * size);
    DrawSolidCube(1.0f);
    glPopMatrix();
    if (leafType == 0) {
        glColor3f(0.2f, 0.6f, 0.3f);
        glPushMatrix();
        glTranslatef(0, 1.8f * size, 0);
        DrawSphere(1.8f * size, 16, 16);
        glPopMatrix();
        glColor3f(0.15f, 0.5f, 0.25f);
        glPushMatrix();
        glTranslatef(0, 2.0f * size, 0);
        DrawSphere(1.2f * size, 12, 12);
        glPopMatrix();
    }
    else if (leafType == 1) {
        glColor3f(0.15f, 0.45f, 0.2f);
        for (int level = 0; level < 4; level++) {
            glPushMatrix();
            glTranslatef(0, (1.0f + level * 0.5f) * size, 0);
            glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
            gluCylinder(gluNewQuadric(), (1.5f - level * 0.3f) * size, 0.0f, 1.0f * size, 8, 1);
            glPopMatrix();
        }
    }
    else {
        glColor3f(0.25f, 0.55f, 0.25f);
        for (int i = 0; i < 3; i++) {
            float angle = i * 120.0f * 3.14159f / 180.0f;
            glPushMatrix();
            glTranslatef(cos(angle) * 0.8f * size, 1.5f * size, sin(angle) * 0.8f * size);
            DrawSphere(1.2f * size, 12, 12);
            glPopMatrix();
        }
        glColor3f(0.2f, 0.5f, 0.2f);
        glPushMatrix();
        glTranslatef(0, 2.0f * size, 0);
        DrawSphere(1.5f * size, 14, 14);
        glPopMatrix();
    }
    glPopMatrix();
}

void DrawDetailedStreetLight(float x, float y, float z, float size, bool isOn) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glColor3f(0.3f, 0.3f, 0.3f);
    glPushMatrix();
    glScalef(0.25f * size, 6.0f * size, 0.25f * size);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glColor3f(0.4f, 0.4f, 0.4f);
    glPushMatrix();
    glTranslatef(0, 6.2f * size, 0);
    glScalef(1.2f * size, 0.4f * size, 1.2f * size);
    DrawSolidCube(1.0f);
    glPopMatrix();
    if (isOn) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(1.0f, 1.0f, 0.6f, 0.4f);
        glPushMatrix();
        glTranslatef(0, 6.0f * size, 0);
        for (int i = 0; i < 3; i++) {
            glPushMatrix();
            glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
            gluCylinder(gluNewQuadric(), (1.0f + i * 0.3f) * size, 0.0f, (2.0f + i * 0.5f) * size, 8, 1);
            glPopMatrix();
        }
        glPopMatrix();
        glDisable(GL_BLEND);
        glColor3f(1.0f, 1.0f, 0.8f);
        glPushMatrix();
        glTranslatef(0, 6.0f * size, 0);
        glScalef(0.5f * size, 0.5f * size, 0.5f * size);
        DrawSolidCube(1.0f);
        glPopMatrix();
    }
    else {
        glColor3f(0.6f, 0.6f, 0.6f);
        glPushMatrix();
        glTranslatef(0, 6.0f * size, 0);
        glScalef(0.5f * size, 0.5f * size, 0.5f * size);
        DrawSolidCube(1.0f);
        glPopMatrix();
    }
    glPopMatrix();
}

void DrawBench(float x, float y, float z, float size) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glColor3f(0.6f, 0.4f, 0.2f);
    glPushMatrix();
    glTranslatef(0, 0.5f * size, 0);
    glScalef(2.5f * size, 0.1f * size, 0.6f * size);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0, 1.0f * size, -0.25f * size);
    glScalef(2.5f * size, 1.0f * size, 0.1f * size);
    DrawSolidCube(1.0f);
    glPopMatrix();
    for (int i = -1; i <= 1; i += 2) {
        for (int j = -1; j <= 1; j += 2) {
            glPushMatrix();
            glTranslatef(i * 1.0f * size, 0.25f * size, j * 0.2f * size);
            glScalef(0.1f * size, 0.5f * size, 0.1f * size);
            DrawSolidCube(1.0f);
            glPopMatrix();
        }
    }
    glPopMatrix();
}

void DrawBirdModel(float x, float y, float z, float size, float rotation) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(rotation * 180.0f / 3.14159f, 0, 1, 0);
    glColor3f(0.3f, 0.3f, 0.3f);
    glPushMatrix();
    glScalef(1.0f * size, 0.5f * size, 0.8f * size);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0.6f * size, 0.2f * size, 0);
    glScalef(0.4f * size, 0.4f * size, 0.4f * size);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glColor3f(0.4f, 0.4f, 0.4f);
    glPushMatrix();
    glTranslatef(0, 0, 0.5f * size);
    glScalef(1.2f * size, 0.1f * size, 0.6f * size);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0, 0, -0.5f * size);
    glScalef(1.2f * size, 0.1f * size, 0.6f * size);
    DrawSolidCube(1.0f);
    glPopMatrix();
    glPopMatrix();
}

void DrawTrees() {
    for (int i = 0; i < 30; i++) {
        DrawDetailedTree(trees[i].x, trees[i].y, trees[i].z, trees[i].size, trees[i].leafType);
    }
}

void DrawStreetLights() {
    for (int i = 0; i < 12; i++) {
        DrawDetailedStreetLight(streetLights[i].x, streetLights[i].y, streetLights[i].z,
            streetLights[i].topSize, streetLights[i].isOn);
    }
}

void DrawBirds() {
    for (int i = 0; i < 20; i++) {
        DrawBirdModel(birds[i].x, birds[i].y, birds[i].z, birds[i].size, birds[i].angle);
    }
}

void DrawDecorations() {
    if (!enableEnvironment) return;
    DrawTrees();
    DrawStreetLights();
    DrawBirds();
    for (int i = 0; i < 50; i++) {
        glPushMatrix();
        glTranslatef(decorations[i].x, decorations[i].y, decorations[i].z);
        glRotatef(decorations[i].rotation * 180.0f / 3.14159f, 0, 1, 0);
        float sway = sin(decorations[i].swayAngle) * 0.1f;
        glRotatef(sway * 10.0f, 0, 1, 0);
        switch (decorations[i].type) {
        case 0:
            glColor3f(0.3f, 0.5f, 0.2f);
            glPushMatrix();
            glTranslatef(0, 1.0f * decorations[i].size, 0);
            DrawSphere(0.8f * decorations[i].size, 10, 10);
            glPopMatrix();
            break;
        case 1:
            DrawDetailedStreetLight(0, 0, 0, decorations[i].size * 0.8f, !isDayTime);
            break;
        case 2:
            DrawBench(0, 0, 0, decorations[i].size);
            break;
        }
        glPopMatrix();
    }
}

void UpdateTimeSystem() {
    static DWORD lastTimeUpdate = 0;
    DWORD currentTime = GetTickCount();
    if (lastTimeUpdate == 0) lastTimeUpdate = currentTime;
    float deltaTime = min((currentTime - lastTimeUpdate) * 0.001f, 0.1f);
    lastTimeUpdate = currentTime;
    timeOfDay += (timeSpeed / 3600.0f) * deltaTime;
    if (timeOfDay >= 24.0f) timeOfDay -= 24.0f;
    bool newDayTime = (timeOfDay >= 6.0f && timeOfDay < 18.0f);
    if (newDayTime != isDayTime) {
        isDayTime = newDayTime;
        for (int i = 0; i < 12; i++) {
            streetLights[i].isOn = !isDayTime;
        }
        needRedraw = true;
    }
    if (timeOfDay >= 6.0f && timeOfDay <= 18.0f) {
        sunAngle = (timeOfDay - 6.0f) * 15.0f;
        sunVisible = true;
        moonVisible = false;
    }
    else {
        sunVisible = false;
        moonVisible = true;
        moonAngle = (timeOfDay >= 18.0f) ? (timeOfDay - 18.0f) * 15.0f : (timeOfDay + 6.0f) * 15.0f;
    }
    if (enableEnvironment) {
        for (int i = 0; i < 100; i++) {
            clouds[i].x += clouds[i].speed * deltaTime * (isDayTime ? 1.0f : 0.5f);
            clouds[i].rotation += 0.05f * deltaTime;
            clouds[i].alpha = 0.4f + 0.3f * sin(clouds[i].rotation);
            if (clouds[i].x > 2000.0f) clouds[i].x = -2000.0f;
            if (clouds[i].x < -2000.0f) clouds[i].x = 2000.0f;
        }
        for (int i = 0; i < 20; i++) {
            birds[i].x += cos(birds[i].angle) * birds[i].speed * deltaTime * 2.0f;
            birds[i].z += sin(birds[i].angle) * birds[i].speed * deltaTime * 2.0f;
            birds[i].y = birds[i].altitude + sin(birds[i].angle * 2.0f + currentTime * 0.002f) * 0.5f;
            if (fabs(birds[i].x) > 250.0f || fabs(birds[i].z) > 250.0f) {
                birds[i].angle += 3.14159f + ((rand() % 100) - 50) / 100.0f;
            }
        }
        for (int i = 0; i < 50; i++) {
            decorations[i].swayAngle += decorations[i].swaySpeed * deltaTime * 2.0f;
        }
    }
    static int lastHour = -1, lastMinute = -1;
    int hour = (int)timeOfDay;
    int minute = (int)((timeOfDay - hour) * 60);
    if (hour != lastHour || minute != lastMinute) {
        UpdateTimeText();
        lastHour = hour;
        lastMinute = minute;
        needRedraw = true;
    }
}

void SetTimeOfDay(float hour) {
    timeOfDay = hour;
    if (timeOfDay >= 24.0f) timeOfDay = 0.0f;
    isDayTime = (timeOfDay >= 6.0f && timeOfDay < 18.0f);
    for (int i = 0; i < 12; i++) {
        streetLights[i].isOn = !isDayTime;
    }
    if (timeOfDay >= 6.0f && timeOfDay <= 18.0f) {
        sunAngle = (timeOfDay - 6.0f) * 15.0f;
        sunVisible = true;
        moonVisible = false;
    }
    else {
        sunVisible = false;
        moonVisible = true;
        moonAngle = (timeOfDay > 18.0f) ? (timeOfDay - 18.0f) * 15.0f : (timeOfDay + 6.0f) * 15.0f;
    }
    if (g_hTimeSlider) {
        SendMessage(g_hTimeSlider, TBM_SETPOS, TRUE, (int)(timeOfDay * 100));
    }
    UpdateTimeText();
    needRedraw = true;
}

void DrawSun() {
    if (!sunVisible) return;
    glDisable(GL_LIGHTING);
    float radius = 500.0f;
    float angleRad = sunAngle * 3.14159f / 180.0f;
    float sunX = radius * cos(angleRad);
    float sunY = radius * sin(angleRad) + 150.0f;
    glColor3f(1.0f, 0.95f, 0.8f);
    glPushMatrix();
    glTranslatef(sunX, sunY, -200.0f);
    DrawSphere(20.0f, 24, 24);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 0.9f, 0.7f, 0.5f);
    glPushMatrix();
    glScalef(1.5f, 1.5f, 1.5f);
    DrawSphere(20.0f, 20, 20);
    glPopMatrix();
    glColor4f(1.0f, 0.8f, 0.6f, 0.3f);
    glPushMatrix();
    glScalef(2.0f, 2.0f, 2.0f);
    DrawSphere(20.0f, 18, 18);
    glPopMatrix();
    glColor4f(1.0f, 0.9f, 0.6f, 0.2f);
    for (int i = 0; i < 8; i++) {
        float rayAngle = i * 45.0f * 3.14159f / 180.0f;
        float rayLength = 60.0f;
        glBegin(GL_TRIANGLES);
        glVertex3f(0, 0, 0);
        glVertex3f(cos(rayAngle) * rayLength, sin(rayAngle) * rayLength, 0);
        glVertex3f(cos(rayAngle + 0.2f) * rayLength, sin(rayAngle + 0.2f) * rayLength, 0);
        glEnd();
    }
    glDisable(GL_BLEND);
    glPopMatrix();
    glEnable(GL_LIGHTING);
}

void DrawMoon() {
    if (!moonVisible) return;
    glDisable(GL_LIGHTING);
    float radius = 500.0f;
    float angleRad = moonAngle * 3.14159f / 180.0f;
    float moonX = radius * cos(angleRad);
    float moonY = radius * sin(angleRad) + 150.0f;
    glColor3f(0.95f, 0.95f, 0.9f);
    glPushMatrix();
    glTranslatef(moonX, moonY, -200.0f);
    DrawSphere(15.0f, 24, 24);
    glColor3f(0.85f, 0.85f, 0.8f);
    for (int i = 0; i < 5; i++) {
        float craterAngle = i * 72.0f * 3.14159f / 180.0f;
        glPushMatrix();
        glTranslatef(cos(craterAngle) * 10.0f, sin(craterAngle) * 10.0f, 0);
        DrawSphere(2.5f, 8, 8);
        glPopMatrix();
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.9f, 0.9f, 1.0f, 0.25f);
    glPushMatrix();
    glScalef(1.8f, 1.8f, 1.8f);
    DrawSphere(15.0f, 20, 20);
    glPopMatrix();
    glDisable(GL_BLEND);
    glPopMatrix();
    glEnable(GL_LIGHTING);
}

void DrawStars() {
    if (isDayTime) return;
    glDisable(GL_LIGHTING);
    float timeFactor = 1.0f;
    if (timeOfDay >= 18.0f && timeOfDay <= 20.0f) {
        timeFactor = (timeOfDay - 18.0f) / 2.0f;
    }
    else if (timeOfDay >= 4.0f && timeOfDay <= 6.0f) {
        timeFactor = (6.0f - timeOfDay) / 2.0f;
    }
    glPointSize(1.8f);
    glBegin(GL_POINTS);
    static DWORD lastTwinkleTime = 0;
    DWORD currentTime = GetTickCount();
    float twinkleTime = currentTime * 0.001f;
    for (int i = 0; i < 1500; i++) {
        float twinkle = 0.7f + 0.3f * sin(twinkleTime * stars[i].twinkleSpeed + stars[i].twinklePhase);
        float brightness = stars[i].brightness * twinkle * timeFactor;
        float r = brightness;
        float g = brightness * (0.8f + 0.2f * sin(stars[i].twinklePhase));
        float b = brightness * (0.9f + 0.1f * cos(stars[i].twinklePhase));
        glColor3f(r, g, b);
        glVertex3f(stars[i].x, stars[i].y, stars[i].z);
    }
    glEnd();
    glPointSize(3.5f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 1500; i += 20) {
        if (stars[i].brightness > 0.7f) {
            float brightness = stars[i].brightness * timeFactor;
            float twinkle = 0.7f + 0.3f * sin(twinkleTime * stars[i].twinkleSpeed * 1.5f + stars[i].twinklePhase);
            brightness *= twinkle;
            glColor3f(brightness, brightness * 0.9f, brightness * 0.8f);
            glVertex3f(stars[i].x, stars[i].y, stars[i].z);
        }
    }
    glEnd();
    glPointSize(5.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 1500; i += 50) {
        if (stars[i].brightness > 0.9f) {
            float brightness = stars[i].brightness * timeFactor * 1.2f;
            float twinkle = 0.5f + 0.5f * sin(twinkleTime * stars[i].twinkleSpeed * 2.0f + stars[i].twinklePhase);
            brightness *= twinkle;
            glColor3f(brightness, brightness, brightness * 0.7f);
            glVertex3f(stars[i].x, stars[i].y, stars[i].z);
        }
    }
    glEnd();
    glEnable(GL_LIGHTING);
}

void DrawClouds() {
    if (!enableEnvironment) return;
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (isDayTime) {
        for (int i = 0; i < 100; i++) {
            glColor4f(1.0f, 1.0f, 1.0f, clouds[i].alpha);
            glPushMatrix();
            glTranslatef(clouds[i].x, clouds[i].y, clouds[i].z);
            glRotatef(clouds[i].rotation * 180.0f / 3.14159f, 0, 1, 0);
            DrawSphere(clouds[i].size * 0.9f, 12, 12);
            for (int j = 0; j < 6; j++) {
                float angle = j * 60.0f * 3.14159f / 180.0f;
                glPushMatrix();
                glTranslatef(cos(angle) * clouds[i].size * 0.7f, sin(angle) * clouds[i].size * 0.3f,
                    (j % 2 == 0 ? 0.5f : -0.5f) * clouds[i].size * 0.4f);
                DrawSphere(clouds[i].size * 0.6f, 10, 10);
                glPopMatrix();
            }
            glPopMatrix();
        }
    }
    else {
        for (int i = 0; i < 50; i++) {
            glColor4f(0.6f, 0.6f, 0.7f, clouds[i].alpha * 0.4f);
            glPushMatrix();
            glTranslatef(clouds[i].x, clouds[i].y, clouds[i].z);
            glRotatef(clouds[i].rotation * 180.0f / 3.14159f, 0, 1, 0);
            DrawSphere(clouds[i].size * 0.8f, 10, 10);
            for (int j = 0; j < 4; j++) {
                float angle = j * 90.0f * 3.14159f / 180.0f;
                glPushMatrix();
                glTranslatef(cos(angle) * clouds[i].size * 0.6f, sin(angle) * clouds[i].size * 0.2f,
                    (j % 2 == 0 ? 0.5f : -0.5f) * clouds[i].size * 0.3f);
                DrawSphere(clouds[i].size * 0.5f, 8, 8);
                glPopMatrix();
            }
            glPopMatrix();
        }
    }
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void DrawGround() {
    glDisable(GL_LIGHTING);
    float groundSize = max(parkingLot.layoutWidth, parkingLot.layoutDepth) * 2.0f + 100.0f;
    glColor3f(isDayTime ? 0.4f : 0.2f, isDayTime ? 0.45f : 0.25f, isDayTime ? 0.35f : 0.15f);
    glBegin(GL_QUADS);
    glVertex3f(-groundSize, 0.0f, -groundSize);
    glVertex3f(groundSize, 0.0f, -groundSize);
    glVertex3f(groundSize, 0.0f, groundSize);
    glVertex3f(-groundSize, 0.0f, groundSize);
    glEnd();
    glColor3f(isDayTime ? 0.5f : 0.3f, isDayTime ? 0.55f : 0.35f, isDayTime ? 0.45f : 0.25f);
    glBegin(GL_QUADS);
    float parkingGroundWidth = parkingLot.layoutWidth + 20.0f;
    float parkingGroundDepth = parkingLot.layoutDepth + 20.0f;
    glVertex3f(-parkingGroundWidth / 2, 0.01f, -parkingGroundDepth / 2);
    glVertex3f(parkingGroundWidth / 2, 0.01f, -parkingGroundDepth / 2);
    glVertex3f(parkingGroundWidth / 2, 0.01f, parkingGroundDepth / 2);
    glVertex3f(-parkingGroundWidth / 2, 0.01f, parkingGroundDepth / 2);
    glEnd();
    if (enableEnvironment) {
        glColor3f(0.3f, 0.5f, 0.3f);
        glPointSize(1.5f);
        glBegin(GL_POINTS);
        for (int i = 0; i < 800; i++) {
            float x = (rand() % (int)(groundSize * 2)) - groundSize;
            float z = (rand() % (int)(groundSize * 2)) - groundSize;
            if (fabs(x) > parkingGroundWidth / 2 + 15.0f || fabs(z) > parkingGroundDepth / 2 + 15.0f) {
                glVertex3f(x, 0.02f, z);
            }
        }
        glEnd();
    }
    glEnable(GL_LIGHTING);
}

void SetupLighting() {
    if (timeOfDay >= 5.0f && timeOfDay <= 7.0f) {
        float factor = (timeOfDay - 5.0f) / 2.0f;
        GLfloat light0_ambient[] = { 0.4f + 0.3f * factor, 0.4f + 0.3f * factor, 0.3f + 0.4f * factor, 1.0f };
        GLfloat light0_diffuse[] = { 0.8f + 0.2f * factor, 0.7f + 0.2f * factor, 0.6f + 0.3f * factor, 1.0f };
        GLfloat light0_specular[] = { 0.9f, 0.8f, 0.7f, 1.0f };
        float radius = 500.0f;
        float angleRad = sunAngle * 3.14159f / 180.0f;
        GLfloat light0_position[] = { radius * cos(angleRad), radius * sin(angleRad) + 150.0f, -200.0f, 1.0f };
        glLightfv(GL_LIGHT0, GL_AMBIENT, light0_ambient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, light0_specular);
        glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
        GLfloat light1_ambient[] = { 0.2f + 0.2f * factor, 0.2f + 0.2f * factor, 0.2f + 0.2f * factor, 1.0f };
        GLfloat light1_diffuse[] = { 0.4f + 0.2f * factor, 0.4f + 0.2f * factor, 0.4f + 0.2f * factor, 1.0f };
        GLfloat light1_position[] = { 0.0f, 150.0f, 0.0f, 1.0f };
        glLightfv(GL_LIGHT1, GL_AMBIENT, light1_ambient);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
        glLightfv(GL_LIGHT1, GL_POSITION, light1_position);
    }
    else if (timeOfDay >= 17.0f && timeOfDay <= 19.0f) {
        float factor = 1.0f - (timeOfDay - 17.0f) / 2.0f;
        GLfloat light0_ambient[] = { 0.5f + 0.2f * factor, 0.4f + 0.1f * factor, 0.3f, 1.0f };
        GLfloat light0_diffuse[] = { 0.9f * factor, 0.7f * factor, 0.5f * factor, 1.0f };
        GLfloat light0_specular[] = { 0.8f * factor, 0.6f * factor, 0.4f * factor, 1.0f };
        float radius = 500.0f;
        float angleRad = sunAngle * 3.14159f / 180.0f;
        GLfloat light0_position[] = { radius * cos(angleRad), radius * sin(angleRad) + 150.0f, -200.0f, 1.0f };
        glLightfv(GL_LIGHT0, GL_AMBIENT, light0_ambient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, light0_specular);
        glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
        GLfloat light1_ambient[] = { 0.3f * factor, 0.2f * factor, 0.1f, 1.0f };
        GLfloat light1_diffuse[] = { 0.5f * factor, 0.4f * factor, 0.3f, 1.0f };
        GLfloat light1_position[] = { 0.0f, 150.0f, 0.0f, 1.0f };
        glLightfv(GL_LIGHT1, GL_AMBIENT, light1_ambient);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
        glLightfv(GL_LIGHT1, GL_POSITION, light1_position);
    }
    else if (isDayTime) {
        GLfloat light0_ambient[] = { 0.7f, 0.7f, 0.7f, 1.0f };
        GLfloat light0_diffuse[] = { 1.0f, 1.0f, 0.95f, 1.0f };
        GLfloat light0_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float radius = 500.0f;
        float angleRad = sunAngle * 3.14159f / 180.0f;
        GLfloat light0_position[] = { radius * cos(angleRad), radius * sin(angleRad) + 150.0f, -200.0f, 1.0f };
        glLightfv(GL_LIGHT0, GL_AMBIENT, light0_ambient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, light0_specular);
        glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
        GLfloat light1_ambient[] = { 0.4f, 0.4f, 0.4f, 1.0f };
        GLfloat light1_diffuse[] = { 0.6f, 0.6f, 0.6f, 1.0f };
        GLfloat light1_position[] = { 0.0f, 150.0f, 0.0f, 1.0f };
        glLightfv(GL_LIGHT1, GL_AMBIENT, light1_ambient);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
        glLightfv(GL_LIGHT1, GL_POSITION, light1_position);
    }
    else {
        GLfloat light0_ambient[] = { 0.1f, 0.1f, 0.15f, 1.0f };
        GLfloat light0_diffuse[] = { 0.2f, 0.2f, 0.3f, 1.0f };
        float radius = 500.0f;
        float angleRad = moonAngle * 3.14159f / 180.0f;
        GLfloat light0_position[] = { radius * cos(angleRad), radius * sin(angleRad) + 150.0f, -200.0f, 1.0f };
        glLightfv(GL_LIGHT0, GL_AMBIENT, light0_ambient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
        glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
        GLfloat light1_ambient[] = { 0.05f, 0.05f, 0.08f, 1.0f };
        GLfloat light1_diffuse[] = { 0.1f, 0.1f, 0.15f, 1.0f };
        GLfloat light1_position[] = { 0.0f, 80.0f, 0.0f, 1.0f };
        glLightfv(GL_LIGHT1, GL_AMBIENT, light1_ambient);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
        glLightfv(GL_LIGHT1, GL_POSITION, light1_position);
        GLfloat light2_ambient[] = { 0.3f, 0.3f, 0.2f, 1.0f };
        GLfloat light2_diffuse[] = { 0.9f, 0.9f, 0.7f, 1.0f };
        GLfloat light2_position[] = { 0.0f, 10.0f, 0.0f, 1.0f };
        glLightfv(GL_LIGHT2, GL_AMBIENT, light2_ambient);
        glLightfv(GL_LIGHT2, GL_DIFFUSE, light2_diffuse);
        glLightfv(GL_LIGHT2, GL_POSITION, light2_position);
    }
    if (isDayTime) {
        glEnable(GL_LIGHT0);
        glEnable(GL_LIGHT1);
        glDisable(GL_LIGHT2);
    }
    else {
        glEnable(GL_LIGHT0);
        glEnable(GL_LIGHT1);
        glEnable(GL_LIGHT2);
    }
}

void ResetProgram() {
    CleanupParkingLot();
    memset(&keys, 0, sizeof(keys));
    timeOfDay = 10.0f;
    timeSpeed = 3600.0f;
    isDayTime = true;
    sunVisible = false;
    moonVisible = false;
    sunAngle = 0.0f;
    moonAngle = 0.0f;
    enableEnvironment = true;
    cameraPosX = 0.0f;
    cameraPosY = 40.0f;
    cameraPosZ = 60.0f;
    cameraYaw = -90.0f;
    cameraPitch = -30.0f;
    selectedCarPort = -1;
    needRedraw = true;
    InitializeParkingLot(100);
    if (g_hTimeSlider) SendMessage(g_hTimeSlider, TBM_SETPOS, TRUE, (int)(timeOfDay * 100));
    if (g_hSpeedSlider) SendMessage(g_hSpeedSlider, TBM_SETPOS, TRUE, (int)timeSpeed);
    UpdateSpeedText();
    UpdateTimeText();
    if (g_hPlateEdit) SetWindowText(g_hPlateEdit, L"");
    if (g_hQueryValueEdit) SetWindowText(g_hQueryValueEdit, L"");
    if (g_hCapacityEdit) SetWindowText(g_hCapacityEdit, L"100");
    SetStatusText(L"程序已重置到初始状态");
}

void InitializeParkingLot(int capacity) {
    CleanupParkingLot();
    memset(&parkingLot, 0, sizeof(ParkingLot));
    parkingLot.capacity = capacity;
    parkingLot.unit_price = 2.5f;
    parkingLot.car_ports = (CarPort*)calloc(capacity, sizeof(CarPort));
    parkingLot.vehicles_in_lot = (VehicleInfo**)calloc(capacity, sizeof(VehicleInfo*));
    parkingLot.waiting_queue = (VehicleInfo**)calloc(MAX_VEHICLES, sizeof(VehicleInfo*));
    if (!parkingLot.car_ports || !parkingLot.vehicles_in_lot || !parkingLot.waiting_queue) {
        MessageBox(NULL, L"内存分配失败", L"错误", MB_ICONERROR);
        return;
    }
    int side = (int)ceil(sqrt((double)capacity));
    parkingLot.portsPerRow = side;
    parkingLot.portsPerCol = side;
    if (parkingLot.portsPerRow * parkingLot.portsPerCol > capacity) {
        parkingLot.portsPerCol = (capacity + parkingLot.portsPerRow - 1) / parkingLot.portsPerRow;
    }
    float portWidth = 3.5f;
    float portLength = 6.0f;
    float colSpacing = 8.0f;
    float rowSpacing = 10.0f;
    parkingLot.layoutWidth = (parkingLot.portsPerRow - 1) * colSpacing + portWidth;
    parkingLot.layoutDepth = (parkingLot.portsPerCol - 1) * rowSpacing + portLength;
    for (int i = 0; i < capacity; i++) {
        swprintf_s(parkingLot.car_ports[i].id, 32, L"%d", i + 1);
        parkingLot.car_ports[i].state = Empty;
        parkingLot.car_ports[i].width = portWidth;
        parkingLot.car_ports[i].length = portLength;
        parkingLot.car_ports[i].selected = false;
        int row = i / parkingLot.portsPerRow;
        int col = i % parkingLot.portsPerRow;
        parkingLot.car_ports[i].x = (col - parkingLot.portsPerRow / 2.0f + 0.5f) * colSpacing;
        parkingLot.car_ports[i].z = -(row - parkingLot.portsPerCol / 2.0f + 0.5f) * rowSpacing;
    }
    InitializeStars();
    InitializeClouds();
    InitializeDecorations();
    CreateVehicleDisplayLists();
    ResetCamera();
    UpdateParkingLotDisplayList();
    UpdateListView();
    wchar_t msg[256];
    swprintf_s(msg, 256, L"停车场已初始化 - 容量: %d, 布局: %d行 x %d列",
        capacity, parkingLot.portsPerCol, parkingLot.portsPerRow);
    SetStatusText(msg);
}

bool IsPositionInParkingLot(float x, float z) {
    float halfWidth = parkingLot.layoutWidth / 2.0f + 2.0f;
    float halfDepth = parkingLot.layoutDepth / 2.0f + 2.0f;
    if (fabs(x) > halfWidth || fabs(z) > halfDepth) return false;
    for (int i = 0; i < parkingLot.capacity; i++) {
        CarPort* port = &parkingLot.car_ports[i];
        if (fabs(x - port->x) < port->width * 0.6f &&
            fabs(z - port->z) < port->length * 0.6f) {
            return true;
        }
    }
    return false;
}

void UpdateParkingLotDisplayList() {
    if (parkingLotDisplayList) glDeleteLists(parkingLotDisplayList, 1);
    parkingLotDisplayList = 0;
    if (parkingLot.capacity <= 0) return;
    parkingLotDisplayList = glGenLists(1);
    if (!parkingLotDisplayList) return;
    glNewList(parkingLotDisplayList, GL_COMPILE);
    glDisable(GL_LIGHTING);
    DrawGround();
    glLineWidth(1.5f);
    for (int i = 0; i < parkingLot.capacity; i++) {
        CarPort* port = &parkingLot.car_ports[i];
        if (port->selected) glColor3f(1.0f, 0.8f, 0.2f);
        else if (port->state == Full) glColor3f(1.0f, 0.5f, 0.5f);
        else glColor3f(0.9f, 0.9f, 0.9f);
        glBegin(GL_LINE_LOOP);
        glVertex3f(port->x - port->width / 2, 0.05f, port->z - port->length / 2);
        glVertex3f(port->x + port->width / 2, 0.05f, port->z - port->length / 2);
        glVertex3f(port->x + port->width / 2, 0.05f, port->z + port->length / 2);
        glVertex3f(port->x - port->width / 2, 0.05f, port->z + port->length / 2);
        glEnd();
    }
    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
    glEndList();
}

void UpdateSpeedText() {
    if (g_hSpeedText) {
        wchar_t speedStr[64];
        swprintf_s(speedStr, 64, L"时间流速: %.0f倍", timeSpeed);
        SetWindowText(g_hSpeedText, speedStr);
    }
}

void UpdateTimeText() {
    if (g_hTimeText) {
        int hour = (int)timeOfDay;
        int minute = (int)((timeOfDay - hour) * 60);
        wchar_t timeStr[64];
        swprintf_s(timeStr, 64, L"当前时间: %02d:%02d %s",
            hour % 12 == 0 ? 12 : hour % 12, minute, hour >= 12 ? L"PM" : L"AM");
        SetWindowText(g_hTimeText, timeStr);
    }
}

void ToggleEnvironment() {
    enableEnvironment = !enableEnvironment;
    needRedraw = true;
    SetStatusText(enableEnvironment ? L"环境显示已启用" : L"环境显示已禁用");
}

void ToggleTimeSpeed(bool increase) {
    if (increase) timeSpeed = min(timeSpeed * 2.0f, 3600.0f);
    else timeSpeed = max(timeSpeed / 2.0f, 1.0f);
    if (g_hSpeedSlider) SendMessage(g_hSpeedSlider, TBM_SETPOS, TRUE, (int)timeSpeed);
    UpdateSpeedText();
    needRedraw = true;
}

void CleanupParkingLot() {
    if (parkingLot.car_ports) {
        free(parkingLot.car_ports);
        parkingLot.car_ports = NULL;
    }
    if (parkingLot.vehicles_in_lot) {
        for (int i = 0; i < parkingLot.capacity; i++) {
            if (parkingLot.vehicles_in_lot[i]) free(parkingLot.vehicles_in_lot[i]);
        }
        free(parkingLot.vehicles_in_lot);
        parkingLot.vehicles_in_lot = NULL;
    }
    if (parkingLot.waiting_queue) {
        for (int i = 0; i < parkingLot.waiting_count; i++) {
            if (parkingLot.waiting_queue[i]) free(parkingLot.waiting_queue[i]);
        }
        free(parkingLot.waiting_queue);
        parkingLot.waiting_queue = NULL;
    }
    if (parkingLotDisplayList) {
        glDeleteLists(parkingLotDisplayList, 1);
        parkingLotDisplayList = 0;
    }
    for (int i = 0; i < 4; i++) {
        if (carDisplayList[i]) {
            glDeleteLists(carDisplayList[i], 1);
            carDisplayList[i] = 0;
        }
    }
    memset(&parkingLot, 0, sizeof(ParkingLot));
}

int FindVehicleByPlate(const wchar_t* plate) {
    for (int i = 0; i < parkingLot.capacity; i++) {
        if (parkingLot.vehicles_in_lot[i] && wcscmp(parkingLot.vehicles_in_lot[i]->plate_number, plate) == 0) {
            return i;
        }
    }
    for (int i = 0; i < parkingLot.waiting_count; i++) {
        if (parkingLot.waiting_queue[i] && wcscmp(parkingLot.waiting_queue[i]->plate_number, plate) == 0) {
            return -2;
        }
    }
    return -1;
}

int FindEmptyParkingSlot() {
    for (int i = 0; i < parkingLot.capacity; i++) {
        if (parkingLot.car_ports[i].state == Empty) return i;
    }
    return -1;
}

const wchar_t* GetVehicleTypeName(VehicleType type) {
    switch (type) {
    case CAR_SMALL: return L"小汽车";
    case CAR_TRUCK_SMALL: return L"小卡";
    case CAR_TRUCK_MEDIUM: return L"中卡";
    case CAR_TRUCK_LARGE: return L"大卡";
    default: return L"未知";
    }
}

bool CompareVehicleInfo(const VehicleInfo* vehicle, int queryType, const wchar_t* queryValue) {
    if (!vehicle) return false;
    switch (queryType) {
    case 0: return wcscmp(vehicle->plate_number, queryValue) == 0;
    case 1: return vehicle->type == (VehicleType)_wtoi(queryValue);
    case 2: return wcscmp(GetVehicleTypeName(vehicle->type), queryValue) == 0;
    default: return false;
    }
}

void AddVehicle(const wchar_t* plate, VehicleType type) {
    if (FindVehicleByPlate(plate) != -1) {
        wchar_t msg[256];
        swprintf_s(msg, 256, L"车牌号 %s 已存在", plate);
        SetStatusText(msg);
        MessageBox(g_hMainWnd, msg, L"提示", MB_ICONWARNING);
        return;
    }
    VehicleInfo* info = (VehicleInfo*)calloc(1, sizeof(VehicleInfo));
    if (!info) {
        SetStatusText(L"内存分配失败");
        return;
    }
    wcscpy_s(info->plate_number, 32, plate);
    info->type = type;
    info->arrival_time = time(NULL);
    info->on_waiting_queue = false;
    info->parking_position = -1;
    GenerateVehicleColors(type, &info->primary_r, &info->primary_g, &info->primary_b,
        &info->secondary_r, &info->secondary_g, &info->secondary_b,
        &info->accent_r, &info->accent_g, &info->accent_b);
    int slot = FindEmptyParkingSlot();
    if (slot != -1) {
        info->parking_position = slot;
        parkingLot.vehicles_in_lot[slot] = info;
        parkingLot.car_ports[slot].state = Full;
        parkingLot.current_count++;
        wchar_t msg[256];
        swprintf_s(msg, 256, L"车辆 %s 已停入车位 %s", plate, parkingLot.car_ports[slot].id);
        SetStatusText(msg);
        AddVehicleToListView(info, slot, false);
    }
    else {
        info->on_waiting_queue = true;
        if (parkingLot.waiting_count < MAX_VEHICLES) {
            parkingLot.waiting_queue[parkingLot.waiting_count++] = info;
            wchar_t msg[256];
            swprintf_s(msg, 256, L"停车场已满，车辆 %s 进入便道等待", plate);
            SetStatusText(msg);
            AddVehicleToListView(info, parkingLot.waiting_count - 1, true);
        }
        else {
            free(info);
            SetStatusText(L"等待队列已满，无法添加车辆");
            MessageBox(g_hMainWnd, L"等待队列已满，无法添加车辆", L"提示", MB_ICONWARNING);
            return;
        }
    }
    parkingLot.total_vehicles++;
    needRedraw = true;
    UpdateParkingLotDisplayList();
}

bool RemoveVehicle(const wchar_t* plate) {
    int pos = FindVehicleByPlate(plate);
    if (pos == -1) {
        wchar_t msg[256];
        swprintf_s(msg, 256, L"未找到车辆 %s", plate);
        SetStatusText(msg);
        return false;
    }
    if (pos == -2) {
        for (int i = 0; i < parkingLot.waiting_count; i++) {
            if (parkingLot.waiting_queue[i] && wcscmp(parkingLot.waiting_queue[i]->plate_number, plate) == 0) {
                free(parkingLot.waiting_queue[i]);
                for (int j = i; j < parkingLot.waiting_count - 1; j++) {
                    parkingLot.waiting_queue[j] = parkingLot.waiting_queue[j + 1];
                }
                parkingLot.waiting_queue[--parkingLot.waiting_count] = NULL;
                wchar_t msg[256];
                swprintf_s(msg, 256, L"车辆 %s 已从便道中移除", plate);
                SetStatusText(msg);
                UpdateListView();
                return true;
            }
        }
        return false;
    }
    if (!parkingLot.vehicles_in_lot || !parkingLot.vehicles_in_lot[pos]) {
        SetStatusText(L"车辆信息错误");
        return false;
    }
    time_t current_time = time(NULL);
    int duration = (int)difftime(current_time, parkingLot.vehicles_in_lot[pos]->arrival_time) * TIME_MULTIPLIER;
    int hour = duration / 3600;
    int minute = (duration % 3600) / 60;
    float cost = 0.0f;
    if (hour > 0 || minute >= 30) cost = (minute > 0 ? hour + 1 : hour) * parkingLot.unit_price;
    wchar_t info[512];
    if (cost == 0.0f) {
        swprintf_s(info, 512,
            L"尊敬的%s车主:\n停车时长：%d小时%d分钟\n不足30分钟不收费\n停车位置：%s",
            parkingLot.vehicles_in_lot[pos]->plate_number, hour, minute, parkingLot.car_ports[pos].id);
    }
    else {
        swprintf_s(info, 512,
            L"尊敬的%s车主:\n停车时长：%d小时%d分钟\n费用：%.2f元\n停车位置：%s",
            parkingLot.vehicles_in_lot[pos]->plate_number, hour, minute, cost, parkingLot.car_ports[pos].id);
    }
    MessageBox(g_hMainWnd, info, L"取车信息", MB_OK | MB_ICONINFORMATION);
    free(parkingLot.vehicles_in_lot[pos]);
    parkingLot.vehicles_in_lot[pos] = NULL;
    parkingLot.car_ports[pos].state = Empty;
    parkingLot.car_ports[pos].selected = false;
    parkingLot.current_count--;
    if (parkingLot.waiting_count > 0) {
        VehicleInfo* next = parkingLot.waiting_queue[0];
        if (next) {
            next->parking_position = pos;
            next->arrival_time = time(NULL);
            next->on_waiting_queue = false;
            parkingLot.vehicles_in_lot[pos] = next;
            parkingLot.car_ports[pos].state = Full;
            parkingLot.current_count++;
            for (int i = 0; i < parkingLot.waiting_count - 1; i++) {
                parkingLot.waiting_queue[i] = parkingLot.waiting_queue[i + 1];
            }
            parkingLot.waiting_queue[--parkingLot.waiting_count] = NULL;
            wchar_t msg[256];
            swprintf_s(msg, 256, L"便道中车辆 %s 已进入车位 %s", next->plate_number, parkingLot.car_ports[pos].id);
            SetStatusText(msg);
        }
    }
    UpdateListView();
    needRedraw = true;
    UpdateParkingLotDisplayList();
    return true;
}

void QueryVehicle(int queryType, const wchar_t* queryValue) {
    if (wcslen(queryValue) == 0) {
        MessageBox(g_hMainWnd, L"请输入查询条件", L"查询提示", MB_ICONINFORMATION);
        return;
    }
    bool found = false;
    wchar_t result[4096] = L"查询结果:\n\n";
    wchar_t temp[512];
    for (int i = 0; i < parkingLot.capacity; i++) {
        if (parkingLot.vehicles_in_lot[i] && CompareVehicleInfo(parkingLot.vehicles_in_lot[i], queryType, queryValue)) {
            found = true;
            VehicleInfo* v = parkingLot.vehicles_in_lot[i];
            time_t current_time = time(NULL);
            int duration = (int)difftime(current_time, v->arrival_time) * TIME_MULTIPLIER;
            int hour = duration / 3600;
            int minute = (duration % 3600) / 60;
            float estimatedCost = 0.0f;
            if (hour > 0 || minute >= 30) estimatedCost = (minute > 0 ? hour + 1 : hour) * parkingLot.unit_price;
            wchar_t arrivalTime[64];
            struct tm timeinfo;
            localtime_s(&timeinfo, &v->arrival_time);
            wcsftime(arrivalTime, 64, L"%Y-%m-%d %H:%M:%S", &timeinfo);
            swprintf_s(temp, 512, L"车牌号：%s\n车型：%s\n状态：停车场内\n停车位置：%s\n到达时间：%s\n停车时长：%d小时%d分钟\n预估费用：%.2f元\n\n",
                v->plate_number, GetVehicleTypeName(v->type), parkingLot.car_ports[i].id, arrivalTime, hour, minute, estimatedCost);
            wcscat_s(result, 4096, temp);
        }
    }
    for (int i = 0; i < parkingLot.waiting_count; i++) {
        if (parkingLot.waiting_queue[i] && CompareVehicleInfo(parkingLot.waiting_queue[i], queryType, queryValue)) {
            found = true;
            VehicleInfo* v = parkingLot.waiting_queue[i];
            time_t current_time = time(NULL);
            int duration = (int)difftime(current_time, v->arrival_time) * TIME_MULTIPLIER;
            int hour = duration / 3600;
            int minute = (duration % 3600) / 60;
            wchar_t arrivalTime[64];
            struct tm timeinfo;
            localtime_s(&timeinfo, &v->arrival_time);
            wcsftime(arrivalTime, 64, L"%Y-%m-%d %H:%M:%S", &timeinfo);
            swprintf_s(temp, 512, L"车牌号：%s\n车型：%s\n状态：便道等待中\n等待位置：第%d位\n到达时间：%s\n等待时长：%d小时%d分钟\n\n",
                v->plate_number, GetVehicleTypeName(v->type), i + 1, arrivalTime, hour, minute);
            wcscat_s(result, 4096, temp);
        }
    }
    if (!found) MessageBox(g_hMainWnd, L"未找到符合条件的车辆", L"查询结果", MB_ICONINFORMATION);
    else MessageBox(g_hMainWnd, result, L"查询结果", MB_OK | MB_ICONINFORMATION);
}

void EditVehicle(const wchar_t* plate) {
    int pos = FindVehicleByPlate(plate);
    if (pos == -1) {
        MessageBox(g_hMainWnd, L"未找到该车辆", L"编辑车辆", MB_ICONINFORMATION);
        return;
    }
    VehicleInfo* vehicle = NULL;
    if (pos == -2) {
        for (int i = 0; i < parkingLot.waiting_count; i++) {
            if (parkingLot.waiting_queue[i] && wcscmp(parkingLot.waiting_queue[i]->plate_number, plate) == 0) {
                vehicle = parkingLot.waiting_queue[i];
                break;
            }
        }
    }
    else {
        vehicle = parkingLot.vehicles_in_lot[pos];
    }
    if (vehicle) {
        WNDCLASS wcEdit = { 0 };
        wcEdit.lpfnWndProc = EditWndProc;
        wcEdit.hInstance = (HINSTANCE)GetWindowLongPtr(g_hMainWnd, GWLP_HINSTANCE);
        wcEdit.lpszClassName = L"EditWindowClass";
        wcEdit.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcEdit.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcEdit.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        RegisterClass(&wcEdit);
        g_hEditWindow = CreateWindowEx(0,
            L"EditWindowClass",
            L"编辑车辆信息",
            WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME,
            CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
            g_hMainWnd, NULL,
            (HINSTANCE)GetWindowLongPtr(g_hMainWnd, GWLP_HINSTANCE), (LPVOID)vehicle);
        if (!g_hEditWindow) {
            MessageBox(g_hMainWnd, L"无法创建编辑窗口", L"错误", MB_ICONERROR);
            return;
        }
        ShowWindow(g_hEditWindow, SW_SHOW);
    }
}

void UpdateVehicleInfo(VehicleInfo* oldVehicle, const wchar_t* newPlate, VehicleType newType) {
    wcscpy_s(oldVehicle->plate_number, 32, newPlate);
    oldVehicle->type = newType;
    GenerateVehicleColors(newType, &oldVehicle->primary_r, &oldVehicle->primary_g, &oldVehicle->primary_b,
        &oldVehicle->secondary_r, &oldVehicle->secondary_g, &oldVehicle->secondary_b,
        &oldVehicle->accent_r, &oldVehicle->accent_g, &oldVehicle->accent_b);
    UpdateListView();
    needRedraw = true;
    UpdateParkingLotDisplayList();
    SetStatusText(L"车辆信息已更新");
}

void AddVehicleToListView(VehicleInfo* vehicle, int position, bool isWaiting) {
    if (!g_hListView || !vehicle) return;
    LVITEM lvi = { 0 };
    lvi.mask = LVIF_TEXT | LVIF_PARAM;
    lvi.iItem = ListView_GetItemCount(g_hListView);
    lvi.lParam = (LPARAM)vehicle;
    wchar_t posStr[32];
    if (isWaiting) swprintf_s(posStr, 32, L"等待%d", position + 1);
    else wcscpy_s(posStr, 32, parkingLot.car_ports[position].id);
    lvi.pszText = vehicle->plate_number;
    ListView_InsertItem(g_hListView, &lvi);
    wchar_t tempBuffer[64];
    wcscpy_s(tempBuffer, 64, GetVehicleTypeName(vehicle->type));
    ListView_SetItemText(g_hListView, lvi.iItem, 1, tempBuffer);
    ListView_SetItemText(g_hListView, lvi.iItem, 2, posStr);
    wcscpy_s(tempBuffer, 64, isWaiting ? L"便道等待" : L"停车场内");
    ListView_SetItemText(g_hListView, lvi.iItem, 3, tempBuffer);
    wchar_t timeStr[64];
    struct tm timeinfo;
    localtime_s(&timeinfo, &vehicle->arrival_time);
    wcsftime(timeStr, 64, L"%Y-%m-%d %H:%M:%S", &timeinfo);
    ListView_SetItemText(g_hListView, lvi.iItem, 4, timeStr);
}

void UpdateListView() {
    if (!g_hListView) return;
    ListView_DeleteAllItems(g_hListView);
    for (int i = 0; i < parkingLot.capacity; i++) {
        if (parkingLot.vehicles_in_lot && parkingLot.vehicles_in_lot[i]) {
            AddVehicleToListView(parkingLot.vehicles_in_lot[i], i, false);
        }
    }
    for (int i = 0; i < parkingLot.waiting_count; i++) {
        if (parkingLot.waiting_queue && parkingLot.waiting_queue[i]) {
            AddVehicleToListView(parkingLot.waiting_queue[i], i, true);
        }
    }
    wchar_t status[256];
    swprintf_s(status, 256,
        L"停车场状态: 总容量%d | 已停%d | 等待%d | 空位%d | 布局:%d行x%d列",
        parkingLot.capacity,
        parkingLot.current_count,
        parkingLot.waiting_count,
        parkingLot.capacity - parkingLot.current_count,
        parkingLot.portsPerCol,
        parkingLot.portsPerRow);
    SetStatusText(status);
}

void DrawVehicle3D(float x, float y, float z, VehicleType type, float r1, float g1, float b1, float r2, float g2, float b2, float r3, float g3, float b3) {
    if (carDisplayList[type]) {
        glPushMatrix();
        glTranslatef(x, y + 0.5f, z);
        glCallList(carDisplayList[type]);
        glPopMatrix();
        return;
    }
    glPushMatrix();
    glTranslatef(x, y + 0.5f, z);
    float bodyLength, bodyWidth, bodyHeight;
    float wheelRadius, wheelWidth;
    float wheelYOffset, wheelXOffset;
    switch (type) {
    case CAR_SMALL:
        bodyLength = 3.2f; bodyWidth = 1.4f; bodyHeight = 1.2f;
        wheelRadius = 0.25f; wheelWidth = 0.15f;
        wheelYOffset = -0.25f; wheelXOffset = bodyWidth / 2 - wheelRadius - 0.15f;
        DrawCar(bodyLength, bodyWidth, bodyHeight, r1, g1, b1, r2, g2, b2, r3, g3, b3);
        DrawWheel(-wheelXOffset, wheelYOffset, -bodyLength / 2 + wheelRadius + 0.3f, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(wheelXOffset, wheelYOffset, -bodyLength / 2 + wheelRadius + 0.3f, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(-wheelXOffset, wheelYOffset, bodyLength / 2 - wheelRadius - 0.3f, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(wheelXOffset, wheelYOffset, bodyLength / 2 - wheelRadius - 0.3f, wheelRadius, wheelWidth, r3, g3, b3);
        break;
    case CAR_TRUCK_SMALL:
        bodyLength = 4.0f; bodyWidth = 1.6f; bodyHeight = 1.6f;
        wheelRadius = 0.3f; wheelWidth = 0.2f;
        wheelYOffset = -0.3f; wheelXOffset = bodyWidth / 2 - wheelRadius - 0.15f;
        DrawTruck(bodyLength, bodyWidth, bodyHeight, r1, g1, b1, r2, g2, b2, r3, g3, b3, type);
        DrawWheel(-wheelXOffset, wheelYOffset, -bodyLength * 0.4f + wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(wheelXOffset, wheelYOffset, -bodyLength * 0.4f + wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(-wheelXOffset, wheelYOffset, bodyLength * 0.25f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(wheelXOffset, wheelYOffset, bodyLength * 0.25f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        break;
    case CAR_TRUCK_MEDIUM:
        bodyLength = 5.5f; bodyWidth = 2.0f; bodyHeight = 2.0f;
        wheelRadius = 0.35f; wheelWidth = 0.25f;
        wheelYOffset = -0.35f; wheelXOffset = bodyWidth / 2 - wheelRadius - 0.15f;
        DrawTruck(bodyLength, bodyWidth, bodyHeight, r1, g1, b1, r2, g2, b2, r3, g3, b3, type);
        DrawWheel(-wheelXOffset, wheelYOffset, -bodyLength * 0.35f + wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(wheelXOffset, wheelYOffset, -bodyLength * 0.35f + wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(-wheelXOffset, wheelYOffset, bodyLength * 0.2f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(wheelXOffset, wheelYOffset, bodyLength * 0.2f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(-wheelXOffset, wheelYOffset, bodyLength * 0.4f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(wheelXOffset, wheelYOffset, bodyLength * 0.4f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        break;
    case CAR_TRUCK_LARGE:
        bodyLength = 7.0f; bodyWidth = 2.4f; bodyHeight = 2.6f;
        wheelRadius = 0.4f; wheelWidth = 0.3f;
        wheelYOffset = -0.4f; wheelXOffset = bodyWidth / 2 - wheelRadius - 0.15f;
        DrawTruck(bodyLength, bodyWidth, bodyHeight, r1, g1, b1, r2, g2, b2, r3, g3, b3, type);
        DrawWheel(-wheelXOffset, wheelYOffset, -bodyLength * 0.3f + wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(wheelXOffset, wheelYOffset, -bodyLength * 0.3f + wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(-wheelXOffset, wheelYOffset, bodyLength * 0.15f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(wheelXOffset, wheelYOffset, bodyLength * 0.15f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(-wheelXOffset, wheelYOffset, bodyLength * 0.35f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(wheelXOffset, wheelYOffset, bodyLength * 0.35f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(-wheelXOffset, wheelYOffset, bodyLength * 0.55f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        DrawWheel(wheelXOffset, wheelYOffset, bodyLength * 0.55f - wheelRadius, wheelRadius, wheelWidth, r3, g3, b3);
        break;
    }
    glPopMatrix();
}

void DrawParkingLot3D() {
    if (parkingLot.capacity <= 0) return;
    if (parkingLotDisplayList) glCallList(parkingLotDisplayList);
    for (int i = 0; i < parkingLot.capacity; i++) {
        if (parkingLot.vehicles_in_lot && parkingLot.vehicles_in_lot[i]) {
            VehicleInfo* v = parkingLot.vehicles_in_lot[i];
            DrawVehicle3D(parkingLot.car_ports[i].x, 0.0f, parkingLot.car_ports[i].z,
                v->type,
                v->primary_r, v->primary_g, v->primary_b,
                v->secondary_r, v->secondary_g, v->secondary_b,
                v->accent_r, v->accent_g, v->accent_b);
        }
    }
    if (enableEnvironment) DrawDecorations();
}

int GetCarPortAtMousePosition(int mouseX, int mouseY) {
    if (parkingLot.capacity <= 0 || !g_h3DView) return -1;
    RECT rc;
    GetClientRect(g_h3DView, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) return -1;
    if (!wglMakeCurrent(g_hDC, g_hRC)) return -1;
    GLdouble modelview[16], projection[16];
    GLint viewport[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);
    GLdouble winX = (GLdouble)mouseX;
    GLdouble winY = (GLdouble)viewport[3] - (GLdouble)mouseY;
    GLdouble nearX, nearY, nearZ;
    GLdouble farX, farY, farZ;
    gluUnProject(winX, winY, 0.0, modelview, projection, viewport, &nearX, &nearY, &nearZ);
    gluUnProject(winX, winY, 1.0, modelview, projection, viewport, &farX, &farY, &farZ);
    GLdouble rayDirX = farX - nearX;
    GLdouble rayDirY = farY - nearY;
    GLdouble rayDirZ = farZ - nearZ;
    if (fabs(rayDirY) > 0.0001) {
        GLdouble t = -nearY / rayDirY;
        if (t >= 0) {
            GLdouble intersectX = nearX + rayDirX * t;
            GLdouble intersectZ = nearZ + rayDirZ * t;
            for (int i = 0; i < parkingLot.capacity; i++) {
                CarPort* port = &parkingLot.car_ports[i];
                float left = port->x - port->width / 2.0f;
                float right = port->x + port->width / 2.0f;
                float front = port->z - port->length / 2.0f;
                float back = port->z + port->length / 2.0f;
                if (intersectX >= left && intersectX <= right &&
                    intersectZ >= front && intersectZ <= back) {
                    return i;
                }
            }
        }
    }
    return -1;
}

void ShowCarPortInfo(int carPortIndex) {
    if (carPortIndex < 0 || carPortIndex >= parkingLot.capacity) return;
    if (selectedCarPort >= 0 && selectedCarPort < parkingLot.capacity) {
        parkingLot.car_ports[selectedCarPort].selected = false;
    }
    selectedCarPort = carPortIndex;
    parkingLot.car_ports[carPortIndex].selected = true;
    UpdateParkingLotDisplayList();
    needRedraw = true;
    CarPort* port = &parkingLot.car_ports[carPortIndex];
    wchar_t info[1024];
    if (port->state == Empty) {
        swprintf_s(info, 1024,
            L"车位信息\n\n车位号: %s\n状态: 空位\n位置: 第%d行, 第%d列\n坐标: (%.1f, %.1f)\n尺寸: %.1f x %.1f 米\n\n点击其他车位查看更多信息",
            port->id,
            (carPortIndex / parkingLot.portsPerRow) + 1,
            (carPortIndex % parkingLot.portsPerRow) + 1,
            port->x, port->z,
            port->width, port->length);
    }
    else {
        VehicleInfo* vehicle = parkingLot.vehicles_in_lot[carPortIndex];
        if (vehicle) {
            time_t current_time = time(NULL);
            int duration = (int)difftime(current_time, vehicle->arrival_time) * TIME_MULTIPLIER;
            int hour = duration / 3600;
            int minute = (duration % 3600) / 60;
            wchar_t arrivalTime[64];
            struct tm timeinfo;
            localtime_s(&timeinfo, &vehicle->arrival_time);
            wcsftime(arrivalTime, 64, L"%Y-%m-%d %H:%M:%S", &timeinfo);
            float estimatedCost = 0.0f;
            if (hour > 0 || minute >= 30) estimatedCost = (minute > 0 ? hour + 1 : hour) * parkingLot.unit_price;
            swprintf_s(info, 1024,
                L"车位信息\n\n车位号: %s\n状态: 已占用\n车牌号: %s\n车型: %s\n到达时间: %s\n停车时长: %d小时%d分钟\n预估费用: %.2f元\n位置: 第%d行, 第%d列\n坐标: (%.1f, %.1f)\n\n点击其他车位查看更多信息",
                port->id,
                vehicle->plate_number,
                GetVehicleTypeName(vehicle->type),
                arrivalTime,
                hour, minute,
                estimatedCost,
                (carPortIndex / parkingLot.portsPerRow) + 1,
                (carPortIndex % parkingLot.portsPerRow) + 1,
                port->x, port->z);
        }
        else swprintf_s(info, 1024, L"车位信息\n\n车位号: %s\n状态: 数据错误", port->id);
    }
    MessageBox(g_hMainWnd, info, L"车位详细信息", MB_OK | MB_ICONINFORMATION);
}

void Render3DView() {
    if (!g_hDC || !g_hRC) return;
    if (!wglMakeCurrent(g_hDC, g_hRC)) return;
    if (timeOfDay >= 5.0f && timeOfDay <= 7.0f) {
        float factor = (timeOfDay - 5.0f) / 2.0f;
        float r = 0.2f + 0.4f * factor;
        float g = 0.3f + 0.5f * factor;
        float b = 0.4f + 0.6f * factor;
        glClearColor(r, g, b, 1.0f);
    }
    else if (timeOfDay >= 17.0f && timeOfDay <= 19.0f) {
        float factor = 1.0f - (timeOfDay - 17.0f) / 2.0f;
        float r = 0.6f + 0.3f * factor;
        float g = 0.4f + 0.3f * factor;
        float b = 0.2f + 0.1f * factor;
        glClearColor(r, g, b, 1.0f);
    }
    else if (isDayTime) {
        float sunHeight = sin(sunAngle * 3.14159f / 180.0f);
        if (sunHeight > 0.7f) glClearColor(0.6f, 0.85f, 1.0f, 1.0f);
        else if (sunHeight > 0.3f) glClearColor(0.5f, 0.78f, 0.98f, 1.0f);
        else glClearColor(0.4f, 0.65f, 0.95f, 1.0f);
    }
    else {
        float nightFactor = 1.0f - fmin(timeOfDay >= 18.0f ? (timeOfDay - 18.0f) / 6.0f : (timeOfDay + 6.0f) / 6.0f, 1.0f);
        glClearColor(0.05f * nightFactor, 0.05f * nightFactor, 0.15f * nightFactor, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    RECT rc;
    GetClientRect(g_h3DView, &rc);
    float aspect = (float)rc.right / (float)rc.bottom;
    if (rc.bottom == 0) aspect = 1.0f;
    gluPerspective(45.0f, aspect, 0.1f, 1000.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float frontX = cos(cameraYaw * 3.14159f / 180.0f) * cos(cameraPitch * 3.14159f / 180.0f);
    float frontY = sin(cameraPitch * 3.14159f / 180.0f);
    float frontZ = sin(cameraYaw * 3.14159f / 180.0f) * cos(cameraPitch * 3.14159f / 180.0f);
    gluLookAt(cameraPosX, cameraPosY, cameraPosZ,
        cameraPosX + frontX, cameraPosY + frontY, cameraPosZ + frontZ,
        0.0f, 1.0f, 0.0f);
    SetupLighting();
    if (isDayTime) {
        DrawSun();
        DrawClouds();
    }
    else {
        DrawMoon();
        DrawStars();
    }
    DrawParkingLot3D();
    glFlush();
    SwapBuffers(g_hDC);
    lastRenderTime = GetTickCount();
}

void ShowStatistics() {
    wchar_t stats[2048];
    float totalIncome = 0.0f;
    int carCount = 0, smallTruckCount = 0, mediumTruckCount = 0, largeTruckCount = 0;
    time_t current_time = time(NULL);
    for (int i = 0; i < parkingLot.capacity; i++) {
        if (parkingLot.vehicles_in_lot[i]) {
            VehicleInfo* v = parkingLot.vehicles_in_lot[i];
            switch (v->type) {
            case CAR_SMALL: carCount++; break;
            case CAR_TRUCK_SMALL: smallTruckCount++; break;
            case CAR_TRUCK_MEDIUM: mediumTruckCount++; break;
            case CAR_TRUCK_LARGE: largeTruckCount++; break;
            }
            int duration = (int)difftime(current_time, v->arrival_time) * TIME_MULTIPLIER;
            int hour = duration / 3600;
            int minute = (duration % 3600) / 60;
            if (hour > 0 || minute >= 30) totalIncome += (minute > 0 ? hour + 1 : hour) * parkingLot.unit_price;
        }
    }
    int waitingCarCount = 0, waitingSmallTruckCount = 0, waitingMediumTruckCount = 0, waitingLargeTruckCount = 0;
    for (int i = 0; i < parkingLot.waiting_count; i++) {
        if (parkingLot.waiting_queue[i]) {
            switch (parkingLot.waiting_queue[i]->type) {
            case CAR_SMALL: waitingCarCount++; break;
            case CAR_TRUCK_SMALL: waitingSmallTruckCount++; break;
            case CAR_TRUCK_MEDIUM: waitingMediumTruckCount++; break;
            case CAR_TRUCK_LARGE: waitingLargeTruckCount++; break;
            }
        }
    }
    float occupancyRate = parkingLot.capacity > 0 ? ((float)parkingLot.current_count / parkingLot.capacity) * 100.0f : 0.0f;
    swprintf_s(stats, 2048,
        L"停车场统计信息\n\n总体统计信息\n总车辆数: %d辆        停车费单价: %.1f元/小时\n停车场内: %d辆        便道等待: %d辆\n空余车位: %d个        占用率: %.1f%%\n停车场布局: %d行×%d列    时间加速: %.0f倍现实时间\n预估总收入: %.2f元\n\n车型分布统计\n小汽车: %d辆 (停车场内: %d, 便道等待: %d)\n小卡:   %d辆 (停车场内: %d, 便道等待: %d)\n中卡:   %d辆 (停车场内: %d, 便道等待: %d)\n大卡:   %d辆 (停车场内: %d, 便道等待: %d)\n\n当前时间: %02d:%02d %s | %s | 环境显示: %s",
        parkingLot.total_vehicles, parkingLot.unit_price,
        parkingLot.current_count, parkingLot.waiting_count,
        parkingLot.capacity - parkingLot.current_count, occupancyRate,
        parkingLot.portsPerCol, parkingLot.portsPerRow, timeSpeed,
        totalIncome,
        carCount + waitingCarCount, carCount, waitingCarCount,
        smallTruckCount + waitingSmallTruckCount, smallTruckCount, waitingSmallTruckCount,
        mediumTruckCount + waitingMediumTruckCount, mediumTruckCount, waitingMediumTruckCount,
        largeTruckCount + waitingLargeTruckCount, largeTruckCount, waitingLargeTruckCount,
        (int)timeOfDay % 12 == 0 ? 12 : (int)timeOfDay % 12,
        (int)((timeOfDay - (int)timeOfDay) * 60),
        (int)timeOfDay >= 12 ? L"PM" : L"AM",
        isDayTime ? L"白天" : L"夜晚",
        enableEnvironment ? L"开启" : L"关闭");
    MessageBox(g_hMainWnd, stats, L"统计信息", MB_OK | MB_ICONINFORMATION);
}

void SaveToFile() {
    FILE* file = _wfopen(L"parking_data.dat", L"wb");
    if (!file) {
        MessageBox(g_hMainWnd, L"无法创建文件", L"错误", MB_ICONERROR);
        return;
    }
    fwrite(&parkingLot.capacity, sizeof(int), 1, file);
    fwrite(&parkingLot.current_count, sizeof(int), 1, file);
    fwrite(&parkingLot.waiting_count, sizeof(int), 1, file);
    fwrite(&parkingLot.total_vehicles, sizeof(int), 1, file);
    fwrite(&parkingLot.unit_price, sizeof(float), 1, file);
    for (int i = 0; i < parkingLot.capacity; i++) fwrite(&parkingLot.car_ports[i], sizeof(CarPort), 1, file);
    for (int i = 0; i < parkingLot.capacity; i++) {
        char hasVehicle = parkingLot.vehicles_in_lot[i] ? 1 : 0;
        fwrite(&hasVehicle, sizeof(char), 1, file);
        if (hasVehicle) fwrite(parkingLot.vehicles_in_lot[i], sizeof(VehicleInfo), 1, file);
    }
    for (int i = 0; i < parkingLot.waiting_count; i++) {
        if (parkingLot.waiting_queue[i]) fwrite(parkingLot.waiting_queue[i], sizeof(VehicleInfo), 1, file);
    }
    fclose(file);
    wchar_t msg[256];
    swprintf_s(msg, 256, L"数据已保存到 parking_data.dat\n停车场: %d辆\n等待队列: %d辆",
        parkingLot.current_count, parkingLot.waiting_count);
    MessageBox(g_hMainWnd, msg, L"保存成功", MB_OK | MB_ICONINFORMATION);
    SetStatusText(L"数据保存成功");
}

void LoadFromFile() {
    FILE* file = _wfopen(L"parking_data.dat", L"rb");
    if (!file) {
        MessageBox(g_hMainWnd, L"无法打开文件或文件不存在", L"错误", MB_ICONERROR);
        return;
    }
    CleanupParkingLot();
    int capacity, current_count, waiting_count, total_vehicles;
    float unit_price;
    fread(&capacity, sizeof(int), 1, file);
    fread(&current_count, sizeof(int), 1, file);
    fread(&waiting_count, sizeof(int), 1, file);
    fread(&total_vehicles, sizeof(int), 1, file);
    fread(&unit_price, sizeof(float), 1, file);
    InitializeParkingLot(capacity);
    parkingLot.current_count = current_count;
    parkingLot.waiting_count = waiting_count;
    parkingLot.total_vehicles = total_vehicles;
    parkingLot.unit_price = unit_price;
    for (int i = 0; i < capacity; i++) fread(&parkingLot.car_ports[i], sizeof(CarPort), 1, file);
    for (int i = 0; i < capacity; i++) {
        char hasVehicle;
        fread(&hasVehicle, sizeof(char), 1, file);
        if (hasVehicle) {
            parkingLot.vehicles_in_lot[i] = (VehicleInfo*)calloc(1, sizeof(VehicleInfo));
            if (parkingLot.vehicles_in_lot[i]) {
                fread(parkingLot.vehicles_in_lot[i], sizeof(VehicleInfo), 1, file);
                parkingLot.car_ports[i].state = Full;
            }
        }
    }
    for (int i = 0; i < waiting_count; i++) {
        parkingLot.waiting_queue[i] = (VehicleInfo*)calloc(1, sizeof(VehicleInfo));
        if (parkingLot.waiting_queue[i]) {
            fread(parkingLot.waiting_queue[i], sizeof(VehicleInfo), 1, file);
            parkingLot.waiting_queue[i]->on_waiting_queue = true;
        }
    }
    fclose(file);
    UpdateListView();
    UpdateParkingLotDisplayList();
    needRedraw = true;
    wchar_t msg[256];
    swprintf_s(msg, 256, L"数据已从文件加载\n停车场: %d辆\n等待队列: %d辆",
        parkingLot.current_count, parkingLot.waiting_count);
    MessageBox(g_hMainWnd, msg, L"加载成功", MB_OK | MB_ICONINFORMATION);
    SetStatusText(L"数据加载成功");
}

void SetStatusText(const wchar_t* text) {
    if (g_hStatusText) SetWindowText(g_hStatusText, text);
}

void ShowHelpContent(int category) {
    if (!g_hHelpWindow) return;
    HWND hText = GetDlgItem(g_hHelpWindow, IDC_HELP_TEXT);
    if (!hText) return;
    const wchar_t* content = L"";
    switch (category) {
    case 0: content = L"停车场管理系统 - 软件概述\r\n\r\n【系统简介】\r\n• 3D可视化停车场管理平台\r\n• 集成车辆管理、实时监控、计费统计\r\n• 支持日夜循环与动态环境\r\n• 模块化设计，功能可扩展\r\n\r\n【核心模块】\r\n• 3D场景渲染：OpenGL高性能图形\r\n• 车辆管理：四类车型进出控制\r\n• 数据管理：本地存储与恢复\r\n• 时间系统：可调节流速与日夜切换\r\n• 环境模拟：天体、植被、动态元素\r\n\r\n【技术架构】\r\n• 开发框架：Win32 API + OpenGL\r\n• 渲染引擎：即时模式OpenGL 1.1\r\n• 数据存储：二进制文件序列化\r\n• 界面组件：Windows通用控件\r\n\r\n【应用场景】\r\n• 商业停车场运营管理\r\n• 城市规划与交通仿真\r\n• 计算机图形学教学演示\r\n• 系统设计与开发实践"; break;
    case 1: content = L"停车场管理系统 - 基本操作\r\n\r\n【停车流程】\r\n1. 输入车牌号（支持中英文字符）\r\n2. 选择车型：小汽车/小卡/中卡/大卡\r\n3. 点击'停车'按钮提交请求\r\n4. 系统自动分配车位或进入等待队列\r\n\r\n【取车流程】\r\n1. 输入待取车辆车牌号\r\n2. 点击'取车'按钮执行操作\r\n3. 系统计算停车时长与费用\r\n4. 显示计费信息并释放车位\r\n\r\n【计费规则】\r\n• 免费时段：30分钟内\r\n• 标准费率：2.5元/小时\r\n• 计费方式：超过30分钟按整小时计费\r\n• 时间基准：系统模拟时间（180倍速）\r\n\r\n【等待队列】\r\n• 停车场满时自动启用\r\n• 最大容量：100辆\r\n• 先进先出排队原则\r\n• 有空位时自动补入\r\n\r\n【初始化软件】\r\n• 点击'初始化软件'按钮\r\n• 重置所有系统状态到初始值\r\n• 包括：车辆数据、时间、环境、摄像头位置\r\n• 停车场容量恢复为默认100辆\r\n• 清除所有输入框内容\r\n• 3D视图重置到默认视角"; break;
    case 2: content = L"停车场管理系统 - 3D视图控制\r\n\r\n【视角操作】\r\n• W/S键：前后移动摄像机\r\n• A/D键：左右移动摄像机\r\n• 空格键：摄像机上升\r\n• Ctrl键：摄像机下降\r\n• R键：重置摄像机位置\r\n\r\n【视角旋转】\r\n• 鼠标右键拖动：自由旋转视角\r\n• 鼠标滚轮：视距缩放\r\n• 左键点击车位：查看详细信息\r\n\r\n【环境控制】\r\n• E键：切换环境显示开关\r\n• 日夜自动切换：6:00-18:00为白天\r\n• 路灯照明：夜晚自动开启\r\n• 动态元素：云朵、飞鸟、植被摇摆\r\n\r\n【视图模式】\r\n• 白天模式：阳光、蓝天、白云\r\n• 夜晚模式：月光、星空、路灯\r\n• 黎明黄昏：渐变过渡效果\r\n• 性能优化：可关闭环境渲染"; break;
    case 3: content = L"停车场管理系统 - 时间控制\r\n\r\n【时间系统】\r\n• 模拟时间流速：1-3600倍现实时间\r\n• 时间滑块：0-24小时手动调整\r\n• 实时显示：12小时制带AM/PM标识\r\n• 日夜循环：自动光照与环境切换\r\n\r\n【时间段】\r\n• 白天：6:00-18:00（阳光照明）\r\n• 夜晚：18:00-次日6:00（路灯照明）\r\n• 黎明：5:00-7:00（渐亮效果）\r\n• 黄昏：17:00-19:00（渐暗效果）\r\n\r\n【时间相关功能】\r\n• 停车计费：基于模拟时间计算\r\n• 环境变化：天体位置与光照强度\r\n• 动态元素：云朵移动、鸟类飞行\r\n• 路灯控制：夜晚自动亮起\r\n\r\n【使用建议】\r\n• 演示模式：高时间流速快速演示\r\n• 精确计费：低时间流速精确计算\r\n• 观察细节：暂停时间观察环境变化"; break;
    case 4: content = L"停车场管理系统 - 数据管理\r\n\r\n【数据保存】\r\n• 保存内容：停车场状态、车辆信息\r\n• 文件格式：二进制dat文件\r\n• 保存位置：程序运行目录\r\n• 操作方式：点击'保存数据'按钮\r\n\r\n【数据加载】\r\n• 加载内容：恢复完整停车场状态\r\n• 自动识别：文件格式与版本兼容\r\n• 状态恢复：车辆位置、等待队列\r\n• 操作方式：点击'加载数据'按钮\r\n\r\n【统计信息】\r\n• 总体统计：容量、占用率、车辆数\r\n• 车型分布：四类车型数量与比例\r\n• 财务统计：预估收入与计费统计\r\n• 系统状态：时间、环境、布局信息\r\n\r\n【数据安全】\r\n• 完整性检查：数据格式验证\r\n• 错误恢复：加载失败自动回退\r\n• 备份机制：建议定期保存数据\r\n• 兼容性：向前兼容数据格式"; break;
    case 5: content = L"停车场管理系统 - 车辆查询与编辑\r\n\r\n【车辆查询】\r\n1. 选择查询类型：\r\n   • 按车牌号查询：输入完整的车牌号码\r\n   • 按车型编号查询：输入0-3数字（0=小汽车，1=小卡，2=中卡，3=大卡）\r\n   • 按车型名称查询：输入车型名称（小汽车/小卡/中卡/大卡）\r\n2. 输入查询条件\r\n3. 点击'查询车辆'按钮\r\n4. 系统显示所有符合条件的车辆详细信息\r\n   • 车牌号与车型\r\n   • 当前状态（停车场内/便道等待）\r\n   • 停车/等待位置\r\n   • 到达时间与时长\r\n   • 预估费用（停车场内车辆）\r\n\r\n【车辆编辑】\r\n1. 输入要编辑的车牌号\r\n2. 点击'编辑车辆信息'按钮\r\n3. 在弹出的编辑对话框中：\r\n   • 修改车牌号（确保唯一性）\r\n   • 选择新的车型\r\n   • 确认保存更改\r\n4. 系统自动更新车辆信息并刷新显示\r\n\r\n【注意事项】\r\n• 车牌号具有唯一性，不能重复\r\n• 修改车牌号后原车牌号被释放\r\n• 编辑操作不影响停车时长计算\r\n• 3D视图中的车辆颜色会相应更新"; break;
    case 6: content = L"停车场管理系统 - 停车规则\r\n\r\n【管理原则】\r\n• 先到先服务：按到达顺序分配车位\r\n• 车型统一：所有车型使用相同车位\r\n• 唯一性：每个车牌只能停一辆车\r\n• 自动分配：系统智能分配最佳车位\r\n\r\n【停车流程】\r\n• 入场检查：车牌查重与有效性验证\r\n• 车位分配：优先小编号空车位\r\n• 等待队列：停车场满时进入排队\r\n• 自动补位：取车后等待车辆自动补入\r\n\r\n【取车流程】\r\n• 车辆识别：车牌号唯一标识\r\n• 费用计算：基于模拟时间自动计算\r\n• 信息显示：停车时长与费用详情\r\n• 状态更新：释放车位并更新队列\r\n\r\n【特殊情况处理】\r\n• 重复车牌：系统拒绝并提示\r\n• 队列满员：提示无法添加新车辆\r\n• 无效车牌：输入检查与验证\r\n• 系统异常：自动恢复与错误提示"; break;
    case 7: content = L"停车场管理系统 - 计费规则\r\n\r\n【收费标准】\r\n• 计费单价：2.5元/小时\r\n• 免费时段：30分钟以内\r\n• 计费单位：整小时（向上取整）\r\n• 时间计算：系统模拟时间\r\n\r\n【计费公式】\r\n• 免费条件：停车时长 ≤ 30分钟\r\n• 计费公式：费用 = ceil(小时数) × 2.5元\r\n• 小时计算：总分钟数 ÷ 60\r\n• 向上取整：不足1小时按1小时计\r\n\r\n【计费示例】\r\n• 25分钟停车：免费\r\n• 45分钟停车：2.5元（按1小时）\r\n• 2小时30分停车：7.5元（按3小时）\r\n• 5小时01分停车：12.5元（按5小时）\r\n\r\n【费用显示】\r\n• 取车时显示：详细停车信息\r\n• 统计信息：实时预估总收入\r\n• 计算基准：180倍时间加速\r\n• 费用明细：时长、费率、总额"; break;
    case 8: content = L"停车场管理系统 - 快捷键指导\r\n\r\n【视图控制快捷键】\r\n• W/S键：摄像机前后移动\r\n• A/D键：摄像机左右移动\r\n• 空格键：摄像机上升\r\n• Ctrl键：摄像机下降\r\n• R键：重置摄像机到默认位置\r\n• 鼠标右键拖动：旋转视角\r\n• 鼠标滚轮：缩放视图\r\n• 鼠标左键点击车位：查看车位信息\r\n\r\n【时间控制快捷键】\r\n• T键：时间流速加倍\r\n• Y键：时间流速减半\r\n• 时间滑块：手动调整当前时间\r\n• 流速滑块：调整时间流速倍数\r\n\r\n【功能快捷键】\r\n• E键：切换环境显示开关\r\n• G键：打开/关闭软件指导手册\r\n\r\n【操作提示】\r\n• 所有快捷键在3D视图获得焦点时有效\r\n• 系统状态栏会显示当前操作提示\r\n• 点击车位可查看详细信息\r\n• 使用时间滑块可以手动调整日夜变化"; break;
    case 9: content = L"停车场管理系统 - 程序算法\r\n\r\n【核心算法架构】\r\n• 数据结构：采用数组+链表混合结构存储车辆信息\r\n• 车位分配：基于贪心算法的最近车位优先策略\r\n• 等待队列：FIFO队列实现先进先出管理\r\n• 碰撞检测：空间分割树优化车辆位置查询\r\n\r\n【图形渲染算法】\r\n• 光照模型：Phong光照模型结合时间动态调整\r\n• 场景管理：显示列表+顶点数组优化渲染性能\r\n• LOD技术：根据距离动态调整模型细节等级\r\n• 纹理管理：Atlas纹理集减少OpenGL状态切换\r\n\r\n【时间与动画系统】\r\n• 插值算法：贝塞尔曲线平滑过渡日夜变化\r\n• 物理模拟：简化牛顿力学计算车辆运动轨迹\r\n• 粒子系统：用于云朵、星空等环境元素渲染\r\n• 时间压缩：线性插值实现可调节时间流速\r\n\r\n【数据管理算法】\r\n• 序列化：二进制格式高效存储恢复系统状态\r\n• 查询优化：哈希表加速车牌号查找操作\r\n• 内存管理：池分配器减少动态内存分配开销\r\n• 错误恢复：事务日志确保数据操作原子性\r\n\r\n【性能优化策略】\r\n• 批处理：相同材质模型合并渲染调用\r\n• 视锥剔除：相机视锥外对象不参与渲染\r\n• 延迟更新：非关键数据异步刷新机制\r\n• 多线程：渲染与逻辑分离提高响应速度"; break;
    }
    SetWindowText(hText, content);
}

LRESULT CALLBACK HelpWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static int currentCategory = 0;
    switch (message) {
    case WM_CREATE: {
        g_hHelpContentFont = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");
        g_hHelpTitleFont = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");
        CreateWindow(L"STATIC", L"停车场管理系统 - 软件指导手册",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            10, 10, 980, 50, hWnd, NULL,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        const wchar_t* categories[] = {
            L"软件概述",
            L"基本操作",
            L"3D视图控制",
            L"时间控制",
            L"数据管理",
            L"车辆查询与编辑",
            L"停车规则",
            L"计费规则",
            L"快捷键指导",
            L"程序算法"
        };
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 2; j++) {
                int index = i * 2 + j;
                if (index < 10) {
                    CreateWindow(L"BUTTON", categories[index],
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        20 + j * 480, 70 + i * 50,
                        460, 40, hWnd, (HMENU)(IDC_HELP_CATEGORY_BUTTON_START + index),
                        (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
                }
            }
        }
        CreateWindow(L"BUTTON", L"返回主界面",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 330, 240, 40, hWnd, (HMENU)IDC_HELP_BACK_BUTTON,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        HWND hText = CreateWindow(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY |
            WS_VSCROLL | ES_AUTOVSCROLL | ES_WANTRETURN,
            20, 390, 960, 340, hWnd, (HMENU)IDC_HELP_TEXT,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        HWND hwndChild = GetWindow(hWnd, GW_CHILD);
        while (hwndChild) {
            if (hwndChild == GetDlgItem(hWnd, 0)) SendMessage(hwndChild, WM_SETFONT, (WPARAM)g_hHelpTitleFont, TRUE);
            else if (hwndChild == hText) {
                SendMessage(hwndChild, WM_SETFONT, (WPARAM)g_hHelpContentFont, TRUE);
                SendMessage(hwndChild, EM_SETTABSTOPS, 1, (LPARAM)40);
            }
            else SendMessage(hwndChild, WM_SETFONT, (WPARAM)g_hHelpContentFont, TRUE);
            hwndChild = GetWindow(hwndChild, GW_HWNDNEXT);
        }
        ShowHelpContent(0);
        break;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId >= IDC_HELP_CATEGORY_BUTTON_START && wmId < IDC_HELP_CATEGORY_BUTTON_START + 10) {
            currentCategory = wmId - IDC_HELP_CATEGORY_BUTTON_START;
            ShowHelpContent(currentCategory);
        }
        else if (wmId == IDC_HELP_BACK_BUTTON) ShowWindow(hWnd, SW_HIDE);
        break;
    }
    case WM_CLOSE: ShowWindow(hWnd, SW_HIDE); break;
    case WM_DESTROY:
        if (g_hHelpTitleFont) { DeleteObject(g_hHelpTitleFont); g_hHelpTitleFont = NULL; }
        if (g_hHelpContentFont) { DeleteObject(g_hHelpContentFont); g_hHelpContentFont = NULL; }
        break;
    default: return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK EditWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static VehicleInfo* editingVehicle = NULL;
    static wchar_t originalPlate[32];
    switch (message) {
    case WM_CREATE: {
        editingVehicle = (VehicleInfo*)((CREATESTRUCT*)lParam)->lpCreateParams;
        if (!editingVehicle) {
            DestroyWindow(hWnd);
            return -1;
        }
        wcscpy_s(originalPlate, 32, editingVehicle->plate_number);
        CreateWindow(L"STATIC", L"修改成的车牌号:", WS_CHILD | WS_VISIBLE,
            30, 30, 120, 30, hWnd, NULL,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        HWND hPlateEdit = CreateWindow(L"EDIT", editingVehicle->plate_number,
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            160, 30, 200, 30, hWnd, (HMENU)IDC_EDIT_PLATE_EDIT,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        CreateWindow(L"STATIC", L"修改成的车型:", WS_CHILD | WS_VISIBLE,
            30, 80, 120, 30, hWnd, NULL,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        HWND hTypeCombo = CreateWindow(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
            160, 80, 200, 200, hWnd, (HMENU)IDC_EDIT_TYPE_COMBO,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        SendMessage(hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"小汽车");
        SendMessage(hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"小卡");
        SendMessage(hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"中卡");
        SendMessage(hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"大卡");
        SendMessage(hTypeCombo, CB_SETCURSEL, editingVehicle->type, 0);
        CreateWindow(L"BUTTON", L"确定",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            80, 150, 120, 40, hWnd, (HMENU)IDC_EDIT_OK_BUTTON,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        CreateWindow(L"BUTTON", L"取消",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            220, 150, 120, 40, hWnd, (HMENU)IDC_EDIT_CANCEL_BUTTON,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        HWND hwndChild = GetWindow(hWnd, GW_CHILD);
        while (hwndChild) {
            SendMessage(hwndChild, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            hwndChild = GetWindow(hwndChild, GW_HWNDNEXT);
        }
        return 0;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId == IDC_EDIT_OK_BUTTON) {
            wchar_t newPlate[32];
            GetDlgItemText(hWnd, IDC_EDIT_PLATE_EDIT, newPlate, 32);
            if (wcslen(newPlate) == 0) {
                MessageBox(hWnd, L"请输入车牌号", L"提示", MB_ICONWARNING);
                return 0;
            }
            if (wcscmp(newPlate, originalPlate) != 0) {
                if (FindVehicleByPlate(newPlate) != -1) {
                    MessageBox(hWnd, L"车牌号已存在", L"错误", MB_ICONERROR);
                    return 0;
                }
            }
            VehicleType newType = (VehicleType)SendDlgItemMessage(hWnd, IDC_EDIT_TYPE_COMBO, CB_GETCURSEL, 0, 0);
            UpdateVehicleInfo(editingVehicle, newPlate, newType);
            DestroyWindow(hWnd);
            g_hEditWindow = NULL;
            return 0;
        }
        else if (wmId == IDC_EDIT_CANCEL_BUTTON) {
            DestroyWindow(hWnd);
            g_hEditWindow = NULL;
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hWnd);
        g_hEditWindow = NULL;
        break;
    case WM_DESTROY:
        g_hEditWindow = NULL;
        break;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

void ShowHelpWindow() {
    if (g_hHelpWindow && IsWindowVisible(g_hHelpWindow)) {
        ShowWindow(g_hHelpWindow, SW_HIDE);
        return;
    }
    if (!g_hHelpWindow) {
        WNDCLASS wcHelp = { 0 };
        wcHelp.lpfnWndProc = HelpWndProc;
        wcHelp.hInstance = (HINSTANCE)GetWindowLongPtr(g_hMainWnd, GWLP_HINSTANCE);
        wcHelp.lpszClassName = L"HelpWindowClass";
        wcHelp.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcHelp.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcHelp.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        RegisterClass(&wcHelp);
        g_hHelpWindow = CreateWindowEx(0,
            L"HelpWindowClass",
            L"停车场管理系统 - 软件指导手册",
            WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME,
            CW_USEDEFAULT, CW_USEDEFAULT, 1000, 800,
            g_hMainWnd, NULL,
            (HINSTANCE)GetWindowLongPtr(g_hMainWnd, GWLP_HINSTANCE), NULL);
        if (!g_hHelpWindow) {
            MessageBox(g_hMainWnd, L"无法创建帮助窗口", L"错误", MB_ICONERROR);
            return;
        }
    }
    ShowWindow(g_hHelpWindow, SW_SHOW);
    SetForegroundWindow(g_hHelpWindow);
}

LRESULT CALLBACK View3DWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        Setup3DView(hWnd);
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        DWORD currentTime = GetTickCount();
        if (needRedraw || (currentTime - lastRenderTime) > 16) {
            Render3DView();
            needRedraw = false;
        }
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (height == 0) height = 1;
        if (g_hDC && g_hRC) {
            wglMakeCurrent(g_hDC, g_hRC);
            glViewport(0, 0, width, height);
        }
        needRedraw = true;
        InvalidateRect(hWnd, NULL, TRUE);
        break;
    }
    case WM_LBUTTONDOWN: {
        SetFocus(hWnd);
        leftMousePressed = true;
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        int carPortIndex = GetCarPortAtMousePosition(x, y);
        if (carPortIndex != -1) ShowCarPortInfo(carPortIndex);
        lastMouseX = x;
        lastMouseY = y;
        break;
    }
    case WM_LBUTTONUP: leftMousePressed = false; break;
    case WM_RBUTTONDOWN:
        SetFocus(hWnd);
        SetCapture(hWnd);
        rightMousePressed = true;
        lastMouseX = GET_X_LPARAM(lParam);
        lastMouseY = GET_Y_LPARAM(lParam);
        break;
    case WM_RBUTTONUP:
        ReleaseCapture();
        rightMousePressed = false;
        break;
    case WM_MOUSEMOVE:
        if (rightMousePressed) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            float xoffset = (float)(x - lastMouseX);
            float yoffset = (float)(lastMouseY - y);
            lastMouseX = x;
            lastMouseY = y;
            cameraYaw += xoffset * 0.3f;
            cameraPitch += yoffset * 0.3f;
            if (cameraPitch > 89.0f) cameraPitch = 89.0f;
            if (cameraPitch < -89.0f) cameraPitch = -89.0f;
            needRedraw = true;
            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;
    case WM_MOUSEWHEEL: {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        float radYaw = cameraYaw * 3.14159f / 180.0f;
        float radPitch = cameraPitch * 3.14159f / 180.0f;
        float frontX = cos(radYaw) * cos(radPitch);
        float frontY = sin(radPitch);
        float frontZ = sin(radYaw) * cos(radPitch);
        cameraPosX += frontX * delta * 0.05f;
        cameraPosY += frontY * delta * 0.05f;
        cameraPosZ += frontZ * delta * 0.05f;
        needRedraw = true;
        InvalidateRect(hWnd, NULL, FALSE);
        break;
    }
    case WM_KEYDOWN:
        SetFocus(hWnd);
        switch (wParam) {
        case 'W': keys.w = true; break;
        case 'S': keys.s = true; break;
        case 'A': keys.a = true; break;
        case 'D': keys.d = true; break;
        case VK_SPACE: keys.space = true; break;
        case VK_CONTROL: keys.ctrl = true; break;
        case 'R': keys.r = true; break;
        case 'G': keys.g = true; break;
        case 'E': keys.e = true; break;
        case 'T': keys.t = true; break;
        case 'Y': keys.y = true; break;
        }
        ProcessCameraInput();
        needRedraw = true;
        InvalidateRect(hWnd, NULL, FALSE);
        break;
    case WM_KEYUP:
        switch (wParam) {
        case 'W': keys.w = false; break;
        case 'S': keys.s = false; break;
        case 'A': keys.a = false; break;
        case 'D': keys.d = false; break;
        case VK_SPACE: keys.space = false; break;
        case VK_CONTROL: keys.ctrl = false; break;
        case 'R': keys.r = false; break;
        case 'G': keys.g = false; break;
        case 'E': keys.e = false; break;
        case 'T': keys.t = false; break;
        case 'Y': keys.y = false; break;
        }
        break;
    case WM_KILLFOCUS: memset(&keys, 0, sizeof(keys)); break;
    case WM_TIMER:
        if (wParam == 1) {
            UpdateTimeSystem();
            if (keys.w || keys.s || keys.a || keys.d || keys.space || keys.ctrl) ProcessCameraInput();
            if (needRedraw) InvalidateRect(hWnd, NULL, FALSE);
        }
        break;
    case WM_DESTROY:
        if (g_hRC) {
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(g_hRC);
            g_hRC = NULL;
        }
        if (g_hDC) {
            ReleaseDC(hWnd, g_hDC);
            g_hDC = NULL;
        }
        break;
    default: return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        g_hFont = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");
        g_hTitleFont = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");

        CreateWindow(L"STATIC", L"停车场管理系统", WS_CHILD | WS_VISIBLE | SS_CENTER,
            10, 10, 1380, 40, hWnd, NULL, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);

        CreateWindow(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
            10, 60, 1380, 180, hWnd, NULL, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);

        int startY = 70;

        CreateWindow(L"STATIC", L"车牌号:", WS_CHILD | WS_VISIBLE,
            30, startY, 80, 30, hWnd, NULL, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        g_hPlateEdit = CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            120, startY, 180, 30, hWnd, (HMENU)IDC_PLATE_EDIT, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);

        CreateWindow(L"STATIC", L"车型:", WS_CHILD | WS_VISIBLE,
            320, startY, 80, 30, hWnd, NULL, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        g_hTypeCombo = CreateWindow(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
            370, startY, 120, 200, hWnd, (HMENU)IDC_TYPE_COMBO, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        SendMessage(g_hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"小汽车");
        SendMessage(g_hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"小卡");
        SendMessage(g_hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"中卡");
        SendMessage(g_hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"大卡");
        SendMessage(g_hTypeCombo, CB_SETCURSEL, 0, 0);

        int buttonWidth = 100;
        int buttonHeight = 30;
        int startX = 510;

        CreateWindow(L"BUTTON", L"停车", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            startX, startY, buttonWidth, buttonHeight, hWnd, (HMENU)IDC_ADD_BUTTON, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += buttonWidth + 10;

        CreateWindow(L"BUTTON", L"取车", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            startX, startY, buttonWidth, buttonHeight, hWnd, (HMENU)IDC_REMOVE_BUTTON, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += buttonWidth + 10;

        CreateWindow(L"BUTTON", L"统计", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            startX, startY, buttonWidth, buttonHeight, hWnd, (HMENU)IDC_STATS_BUTTON, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += buttonWidth + 10;

        CreateWindow(L"BUTTON", L"保存数据", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            startX, startY, buttonWidth, buttonHeight, hWnd, (HMENU)IDC_SAVE_BUTTON, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += buttonWidth + 10;

        CreateWindow(L"BUTTON", L"加载数据", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            startX, startY, buttonWidth, buttonHeight, hWnd, (HMENU)IDC_LOAD_BUTTON, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += buttonWidth + 10;

        CreateWindow(L"BUTTON", L"软件指导(G)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            startX, startY, buttonWidth, buttonHeight, hWnd, (HMENU)IDC_HELP_BUTTON, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += buttonWidth + 10;

        startY += 40;
        startX = 30;

        CreateWindow(L"STATIC", L"查询类型:", WS_CHILD | WS_VISIBLE,
            startX, startY, 80, 30, hWnd, NULL, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += 90;
        g_hQueryTypeCombo = CreateWindow(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
            startX, startY, 180, 200, hWnd, (HMENU)IDC_QUERY_TYPE_COMBO, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        SendMessage(g_hQueryTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"按车牌号查询");
        SendMessage(g_hQueryTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"按车型编号查询");
        SendMessage(g_hQueryTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"按车型名称查询");
        SendMessage(g_hQueryTypeCombo, CB_SETCURSEL, 0, 0);
        startX += 190;

        CreateWindow(L"STATIC", L"查询条件:", WS_CHILD | WS_VISIBLE,
            startX, startY, 80, 30, hWnd, NULL, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += 90;
        g_hQueryValueEdit = CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            startX, startY, 200, 30, hWnd, (HMENU)IDC_QUERY_VALUE_EDIT, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += 210;

        CreateWindow(L"BUTTON", L"查询车辆", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            startX, startY, buttonWidth, buttonHeight, hWnd, (HMENU)IDC_QUERY_BUTTON, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += buttonWidth + 10;

        CreateWindow(L"BUTTON", L"编辑车辆信息", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            startX, startY, 120, buttonHeight, hWnd, (HMENU)IDC_EDIT_BUTTON, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += 130;

        startY += 40;
        startX = 30;

        CreateWindow(L"STATIC", L"系统设置", WS_CHILD | WS_VISIBLE,
            startX, startY, 80, 30, hWnd, NULL, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += 90;

        CreateWindow(L"STATIC", L"停车场容量(1-1000):", WS_CHILD | WS_VISIBLE,
            startX, startY, 180, 30, hWnd, NULL, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += 190;
        g_hCapacityEdit = CreateWindow(L"EDIT", L"100", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
            startX, startY, 100, 30, hWnd, (HMENU)IDC_CAPACITY_EDIT, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += 110;

        CreateWindow(L"BUTTON", L"初始化", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            startX, startY, 100, 30, hWnd, (HMENU)IDC_INIT_BUTTON, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += 110;

        CreateWindow(L"BUTTON", L"初始化软件", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            startX, startY, 120, 30, hWnd, (HMENU)IDC_RESET_BUTTON, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += 130;

        CreateWindow(L"BUTTON", L"环境显示(E)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            startX, startY, 120, 30, hWnd, (HMENU)IDC_ENV_DISPLAY, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);

        startY += 40;
        startX = 30;

        CreateWindow(L"STATIC", L"时间控制:", WS_CHILD | WS_VISIBLE,
            startX, startY, 100, 30, hWnd, NULL, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += 110;

        g_hTimeText = CreateWindow(L"STATIC", L"当前时间: 10:00 AM", WS_CHILD | WS_VISIBLE,
            startX, startY, 120, 30, hWnd, (HMENU)IDC_TIME_TEXT, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += 130;

        g_hSpeedText = CreateWindow(L"STATIC", L"时间流速: 3600倍", WS_CHILD | WS_VISIBLE,
            startX, startY, 120, 30, hWnd, (HMENU)IDC_SPEED_TEXT, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);

        startX = 400;

        CreateWindow(L"STATIC", L"时间滑块(0-24小时):", WS_CHILD | WS_VISIBLE,
            startX, startY, 120, 30, hWnd, NULL, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += 130;

        g_hTimeSlider = CreateWindow(TRACKBAR_CLASS, L"",
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ,
            startX, startY, 300, 30, hWnd, (HMENU)IDC_TIME_SLIDER,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        SendMessage(g_hTimeSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 2400));
        SendMessage(g_hTimeSlider, TBM_SETPOS, TRUE, 1000);
        SendMessage(g_hTimeSlider, TBM_SETTICFREQ, 100, 0);
        startX += 310;

        CreateWindow(L"STATIC", L"流速滑块(1-3600倍):", WS_CHILD | WS_VISIBLE,
            startX, startY, 180, 30, hWnd, NULL, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        startX += 190;

        g_hSpeedSlider = CreateWindow(TRACKBAR_CLASS, L"",
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ,
            startX, startY, 300, 30, hWnd, (HMENU)IDC_SPEED_SLIDER,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        SendMessage(g_hSpeedSlider, TBM_SETRANGE, TRUE, MAKELONG(1, 3600));
        SendMessage(g_hSpeedSlider, TBM_SETPOS, TRUE, 3600);
        SendMessage(g_hSpeedSlider, TBM_SETTICFREQ, 200, 0);

        WNDCLASS wc3d = { 0 };
        wc3d.lpfnWndProc = View3DWndProc;
        wc3d.hInstance = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
        wc3d.lpszClassName = L"View3DClass";
        wc3d.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc3d.hbrBackground = NULL;
        wc3d.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        RegisterClass(&wc3d);
        g_h3DView = CreateWindow(L"View3DClass", L"3D停车场视图",
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
            20, 280, 700, 500, hWnd, (HMENU)IDC_3D_VIEW,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);

        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icex);
        g_hListView = CreateWindow(WC_LISTVIEW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_BORDER | WS_VSCROLL,
            740, 280, 650, 500, hWnd, (HMENU)IDC_MAIN_LIST,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);

        LVCOLUMN lvc;
        lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
        lvc.fmt = LVCFMT_LEFT;
        const wchar_t* columnTitles[] = { L"车牌号", L"车型", L"位置", L"状态", L"到达时间" };
        int widths[] = { 150, 100, 120, 150, 200 };
        wchar_t tempTitle[32];
        for (int i = 0; i < 5; i++) {
            lvc.iSubItem = i;
            lvc.cx = widths[i];
            wcscpy_s(tempTitle, 32, columnTitles[i]);
            lvc.pszText = tempTitle;
            ListView_InsertColumn(g_hListView, i, &lvc);
        }

        g_hStatusText = CreateWindow(L"STATIC",
            L"停车场管理系统 - 鼠标左键点击车位查看详细信息 | G键打开软件指导 | E键切换环境显示 | T/Y键调整时间流速 | R键重置视角 | 点击'初始化软件'重置程序",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 800, 1360, 30, hWnd, (HMENU)IDC_STATUS_TEXT,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);

        HWND hwndChild = GetWindow(hWnd, GW_CHILD);
        while (hwndChild) {
            if (hwndChild == GetDlgItem(hWnd, 0)) SendMessage(hwndChild, WM_SETFONT, (WPARAM)g_hTitleFont, TRUE);
            else SendMessage(hwndChild, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            hwndChild = GetWindow(hwndChild, GW_HWNDNEXT);
        }
        srand((unsigned int)time(NULL));
        InitializeParkingLot(100);
        UpdateSpeedText();
        UpdateTimeText();
        SetTimer(hWnd, 1, 16, NULL);
        break;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDC_ADD_BUTTON: {
            wchar_t plate[32];
            GetWindowText(g_hPlateEdit, plate, 32);
            if (wcslen(plate) == 0) {
                MessageBox(hWnd, L"请输入车牌号", L"提示", MB_OK | MB_ICONINFORMATION);
                break;
            }
            int typeIndex = (int)SendMessage(g_hTypeCombo, CB_GETCURSEL, 0, 0);
            AddVehicle(plate, (VehicleType)typeIndex);
            SetWindowText(g_hPlateEdit, L"");
            break;
        }
        case IDC_REMOVE_BUTTON: {
            wchar_t plate[32];
            GetWindowText(g_hPlateEdit, plate, 32);
            if (wcslen(plate) == 0) {
                MessageBox(hWnd, L"请输入车牌号", L"提示", MB_OK | MB_ICONINFORMATION);
                break;
            }
            RemoveVehicle(plate);
            SetWindowText(g_hPlateEdit, L"");
            break;
        }
        case IDC_QUERY_BUTTON: {
            wchar_t queryValue[128];
            GetWindowText(g_hQueryValueEdit, queryValue, 128);
            int queryType = (int)SendMessage(g_hQueryTypeCombo, CB_GETCURSEL, 0, 0);
            QueryVehicle(queryType, queryValue);
            break;
        }
        case IDC_EDIT_BUTTON: {
            wchar_t plate[32];
            GetWindowText(g_hPlateEdit, plate, 32);
            if (wcslen(plate) == 0) {
                MessageBox(hWnd, L"请输入车牌号", L"提示", MB_OK | MB_ICONINFORMATION);
                break;
            }
            EditVehicle(plate);
            SetWindowText(g_hPlateEdit, L"");
            break;
        }
        case IDC_STATS_BUTTON: ShowStatistics(); break;
        case IDC_SAVE_BUTTON: SaveToFile(); break;
        case IDC_LOAD_BUTTON: LoadFromFile(); break;
        case IDC_INIT_BUTTON: {
            wchar_t capacityW[32];
            GetWindowText(g_hCapacityEdit, capacityW, 32);
            int capacity = _wtoi(capacityW);
            if (capacity > 0 && capacity <= 1000) InitializeParkingLot(capacity);
            else MessageBox(hWnd, L"请输入有效的容量 (1-1000)", L"提示", MB_OK | MB_ICONERROR);
            break;
        }
        case IDC_RESET_BUTTON: ResetProgram(); break;
        case IDC_HELP_BUTTON: ShowHelpWindow(); break;
        case IDC_ENV_DISPLAY: ToggleEnvironment(); break;
        }
        break;
    }
    case WM_HSCROLL: {
        HWND hwndScroll = (HWND)lParam;
        if (hwndScroll == g_hTimeSlider) {
            int pos = SendMessage(g_hTimeSlider, TBM_GETPOS, 0, 0);
            SetTimeOfDay(pos / 100.0f);
        }
        else if (hwndScroll == g_hSpeedSlider) {
            int pos = SendMessage(g_hSpeedSlider, TBM_GETPOS, 0, 0);
            timeSpeed = (float)pos;
            UpdateSpeedText();
            needRedraw = true;
            if (g_h3DView) InvalidateRect(g_h3DView, NULL, FALSE);
        }
        break;
    }
    case WM_SIZE: {
        RECT rcClient;
        GetClientRect(hWnd, &rcClient);
        int clientHeight = rcClient.bottom;
        int clientWidth = rcClient.right;
        if (g_h3DView) {
            SetWindowPos(g_h3DView, NULL, 20, 280,
                clientWidth / 2 - 10, clientHeight - 310, SWP_NOZORDER);
        }
        if (g_hListView) {
            SetWindowPos(g_hListView, NULL, clientWidth / 2 + 20, 280,
                clientWidth / 2 - 40, clientHeight - 310, SWP_NOZORDER);
        }
        if (g_hStatusText) {
            SetWindowPos(g_hStatusText, NULL, 20, clientHeight - 30,
                clientWidth - 40, 25, SWP_NOZORDER);
        }
        break;
    }
    case WM_DESTROY:
        KillTimer(hWnd, 1);
        CleanupParkingLot();
        if (g_hFont) DeleteObject(g_hFont);
        if (g_hTitleFont) DeleteObject(g_hTitleFont);
        PostQuitMessage(0);
        break;
    default: return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    _wsetlocale(LC_ALL, L"chs");
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"ParkingLotSystem";
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    if (!RegisterClassEx(&wcex)) {
        MessageBox(NULL, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        return 1;
    }
    g_hMainWnd = CreateWindowEx(0,
        L"ParkingLotSystem",
        L"停车场管理系统",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1420, 900,
        NULL, NULL, hInstance, NULL);
    if (!g_hMainWnd) {
        MessageBox(NULL, L"窗口创建失败!", L"错误", MB_ICONERROR);
        return 1;
    }
    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
