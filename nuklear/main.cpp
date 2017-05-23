
/*
  Copyright 2017 Simeon Bassett

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

/*

  NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
  The widget drawing code is based on the NanoVG demo application
  by Mikko Mononen.

  All rights reserved. Use of NanoGUI and NanoVG source code is governed by a
  BSD-style license that can be found in the LICENSE.txt file.

*/

// -todo: fix crash on focus event while db list is open - fixed by checking for refCount in the focus?
// -todo: generate .c arrays of bytes from .ttf files

// todo: integrate font files as .h files so that .exe is contained with no dependencies
// todo: try out tiny/nano file dialog for saving and loading of serialized data 
// todo: make save button or menu to save serialized files 
// todo: make a load button or menu to load serialized files
// todo: write visualizer overview for Readme.md  
// todo: make an initial overlay text that has hotkeys and dissapears when the screen is clicked

// todo: make camera fitting use the field of view and change the dist to fit all geometry - use the camera's new position and take a vector orthongonal to the camera-to-lookat vector. the acos of the dot product is the angle, but tan will be needed to set a position from the angle?
// todo: keep databases in memory after listing them?
// todo: make label switches not only turn keys on and off, but fade their transparency too?
// todo: move and rename project to LavaViz or any non test name
// todo: put sensitivity into a separate settings struct
// idea: make IndexedVerts restore without copying bytes? - this would mean that there would need to be a pointer to the head and each member would be pointers filled in on deserialization? 
// idea: call getKeys asynchronously - use futures? 
// idea: put version next to key value 
// idea: test indexed verts with images
// idea: ability to save indexed verts objects to a file
// idea: look into drag and drop to load indexed verts objects by dragging from a file
// idea: make a text visualizer?
//       - display keys and data from db directly - keys then string on one line, click the line and add a tab with the key name in the tab title and a split window between hex and string form?
//       - display values and strings
// idea: make a strings binary format - will this work for an arbitrary packed binary list? - should there be a data structure type and an underlying data type?
//       - first 8 bytes -> total size in bytes
//       - next  4 bytes  -> data structure type? - binary list here?
//       - next  4 bytes  -> underlying data type?
//       - next  8 bytes  -> number of strings
//       - for the number of strings -> 8 bytes for the offset of each string from the start of the whole binary 
//       - next bytes are all string data - does the encoding matter here? should it be utf8 since you know the length of each string?
// idea: work out a type enum for lava data structures? use the upper 16 bits of the size? this leaves 'only' 65,536 different types and 281 terabytes as the max size - use first 8 bytes for size and next 8 bytes for version?
// idea: ability to display points of an indexed verts type as numbers - this would give the ability to have numbers floating in space

#include <chrono>
#include <memory>
#include <algorithm>
#include <fstream>
#include <sstream>
#include "GL/glew.h"
#include "no_rt_util.h"

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/constants.hpp"
#include "glm/gtx/matrix_decompose.hpp"

#include "glfw3.h"

#include "VizData.hpp"
#include "VizGen.hpp"
#include "VizTfm.hpp"

#define ENTRY_DECLARATION int main(void)
#ifdef _MSC_VER
  #pragma comment(lib, "glfw3dll.lib")
  #pragma comment(lib, "opengl32.lib")
  //#pragma comment(lib, "glew32.lib")

  #define USE_CONSOLE                                 // turning this off will use the windows subsystem in the linker and change the entry point to WinMain() so that no command line/console will appear
  #ifndef USE_CONSOLE
    #pragma comment( linker, "/subsystem:windows" )
    #undef ENTRY_DECLARATION
    #define ENTRY_DECLARATION int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
  #endif
#endif

#define MAX_VERTEX_BUFFER  512 * 1024
#define MAX_ELEMENT_BUFFER 128 * 1024


namespace {  // functions that are a transform from one datatype to another are in VizTfm.hpp - functions here are more state based

using v2i  =  Eigen::Vector2i;
using v4f  =  Eigen::Vector4f;

const u32 TITLE_MAX_LEN = 256;

vec3                      pos(mat4 const& m)
{ return m[3];                      }
void                  set_pos(mat4* m, vec3 const p)
{
  //(*m)[0].w = p.x;
  //(*m)[1].w = p.y;
  //(*m)[2].w = p.z;
  (*m)[3].x = p.x;
  (*m)[3].y = p.y;
  (*m)[3].z = p.z;
}
float        wrapAngleRadians(float angle)     
{
  using namespace glm;
  const static float _2PI = 2.f*pi<float>();

  return fmodf(angle, _2PI);
}

void                initSimDB(str const& name)
{
  db.close();
  new (&db) simdb(name.c_str(), 4096, 1 << 14);             // inititialize the DB with placement new into the data segment
}
void                 initGlew()
{
  //glewExperimental = 1;
  if(glewInit() != GLEW_OK) {
    fprintf(stderr, "Failed to setup GLEW\n");
    exit(1);
  }
}
Camera             initCamera()
{
  Camera cam;
  cam.fov             = PIf * 0.5f;
  cam.mouseDelta      = vec2(0.0f, 0.0f);
  cam.btn2Delta       = vec2(0.0f, 0.0f);
  cam.sensitivity     = 0.005f;
  cam.pansense        = 0.002f;
  cam.pos             = vec3(0,0,-10.0f);
  cam.rot             = vec3(0,0,0);
  cam.lookAt          = vec3(0.0f, 0.0f, 0.0f);
  cam.dist            = 10.0f;
  cam.up              = vec3(0.0f, 1.0f, 0.0f);
  cam.oldMousePos     = vec2(0.0f, 0.0f);
  cam.rightButtonDown = false;
  cam.leftButtonDown  = false;
  cam.nearClip        = 0.01f;
  cam.farClip         = 1000.0f;

  return cam;
}

void            errorCallback(int e, const char *d) {
  printf("Error %d: %s\n", e, d);
  fflush(stdout);
}
void              keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  //using namespace glm;

  if(key==GLFW_KEY_ESCAPE && action==GLFW_PRESS)
    glfwSetWindowShouldClose(window, GL_TRUE);

  VizData* vd = (VizData*)glfwGetWindowUserPointer(window);
  auto&   cam = vd->camera;
  switch(key)
  {
  case GLFW_KEY_K: {
  } break; 
  case GLFW_KEY_H: {
    vd->camera = initCamera();
  } break; 
  case GLFW_KEY_J:
  case GLFW_KEY_F: {
    if(action==GLFW_RELEASE){
      vec4   sph   =  shapes_to_bndsph(*vd);
      vec3  cntr   =  vec3(sph.x,sph.y,sph.z);
      vec3  ofst   =  cam.pos - cam.lookAt;
      cam.lookAt   =  cntr;
      cam.pos      =  cam.lookAt + ofst;
    }
    //cam.dist     =  sph.r;
  } break;
  case GLFW_KEY_PAGE_UP: {
    vd->ui.ptSz *= 2;
  } break;
  case GLFW_KEY_PAGE_DOWN: {
    vd->ui.ptSz /= 2;
  } break;
  default:
    break;
  }

  vd->ui.screen.keyCallbackEvent(key, scancode, action, mods);
}
void           scrollCallback(GLFWwindow* window, double xofst, double yofst)
{
  using namespace std;
  
  const float dollySens = 0.1f;

  VizData*   vd = (VizData*)glfwGetWindowUserPointer(window);
  auto&     cam = vd->camera;
  float    ofst = (float) -(cam.dist*yofst*dollySens);
  float  prvDst = cam.dist;                                        // prvDst is previous distance
  float  nxtDst = cam.dist;
  nxtDst        = cam.dist + ofst;
  cam.dist      = nxtDst;

  vd->ui.screen.scrollCallbackEvent(xofst, yofst);
}
void         mouseBtnCallback(GLFWwindow* window, int button, int action, int mods)
{
  VizData* vd = (VizData*)glfwGetWindowUserPointer(window);

  if(button==GLFW_MOUSE_BUTTON_LEFT){
    if(action==GLFW_PRESS) vd->camera.leftButtonDown = true;
    else if(action==GLFW_RELEASE) vd->camera.leftButtonDown = false;
  }

  if(button==GLFW_MOUSE_BUTTON_RIGHT){
    if(action==GLFW_PRESS) vd->camera.rightButtonDown = true;
    else if(action==GLFW_RELEASE){
      vd->camera.rightButtonDown = false;
    }
  }

  vd->ui.screen.mouseButtonCallbackEvent(button, action, mods);
}
void        cursorPosCallback(GLFWwindow* window, double x, double y)
{
  //using namespace glm;
  const static float _2PI = 2.f* PIf; //  pi<float>();

  VizData* vd = (VizData*)glfwGetWindowUserPointer(window);

  Camera& cam = vd->camera;
  vec2 newMousePosition = vec2((float)x, (float)y);

  if(cam.leftButtonDown){
    cam.mouseDelta = (newMousePosition - cam.oldMousePos);
  }else{ cam.mouseDelta = vec2(0,0); }
    
  if(cam.rightButtonDown){
    cam.btn2Delta  = (newMousePosition - cam.oldMousePos);
  }else{ 
    cam.btn2Delta  = vec2(0,0);

    cam.tfm     =  lookAt(cam.pos, cam.lookAt, cam.up);
    vec3 scale, pos, skew; vec4 persp;
    quat rot;
    decompose(cam.tfm, scale, rot, pos, skew, persp);
    cam.pantfm  =  (mat4)conjugate(rot);
  }

  cam.oldMousePos = newMousePosition;

  vd->ui.screen.cursorPosCallbackEvent(x,y);
}
void             charCallback(GLFWwindow* window, unsigned int codepoint)
{
  VizData* vd = (VizData*)glfwGetWindowUserPointer(window);

  vd->ui.screen.charCallbackEvent(codepoint);
}
void             dropCallback(GLFWwindow* window, int count, const char** filenames)
{
  VizData* vd = (VizData*)glfwGetWindowUserPointer(window);

  vd->ui.screen.dropCallbackEvent(count, filenames);
}
void  framebufferSizeCallback(GLFWwindow* window, int w, int h)
{
  VizData* vd = (VizData*)glfwGetWindowUserPointer(window);

  vd->ui.screen.resizeCallbackEvent(w, h);
}
GLFWwindow*          initGLFW(VizData* vd)
{
  glfwSetErrorCallback(errorCallback);
  if( !glfwInit() ){
    fprintf(stdout, "[GFLW] failed to init!\n");
    exit(1);
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_DOUBLEBUFFER, GL_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 32);

  GLFWwindow* win = glfwCreateWindow(vd->ui.w, vd->ui.h, "Visualizer - simdb:test", NULL, NULL);    assert(win!=nullptr);
  glfwMakeContextCurrent(win);
  glfwGetWindowSize(win, &vd->ui.w, &vd->ui.h);

  glfwSetCharCallback(win,              charCallback);         // in glfw charCallback is for typing letters and is different than the keyCallback for keys like backspace 
  glfwSetKeyCallback(win,                keyCallback);
  glfwSetScrollCallback(win,          scrollCallback);
  glfwSetCursorPosCallback(win,    cursorPosCallback);
  glfwSetMouseButtonCallback(win,   mouseBtnCallback);
  glfwSetDropCallback(win,              dropCallback);
  glfwSetFramebufferSizeCallback(win, framebufferSizeCallback);
  //glfwSetScrollCallback(win, )

  #ifdef _WIN32
    //GLFWimage images[2];
    //images[0] = LoadIcon("lava.jpg");
    //images[1] = LoadIcon("lava.jpg");
    //glfwSetWindowIcon(win, 2, images);
  #endif

  return win;
}

void           buttonCallback(str key, bool pushed)
{
  vd.shapes[key].active = pushed;
}
void            dbLstCallback(bool pressed)
{
  if(pressed){
    vd.ui.dbNames = simdb_listDBs();                            // all of these are globals
    if(vd.ui.dbLst){
      vd.ui.dbLst->setItems(vd.ui.dbNames);
    }
    vd.ui.screen.performLayout();
  }
}

void              RenderShape(Shape const& shp, mat4 const& m) // GLuint shaderId)
{
  glUseProgram(shp.shader);  //shader.use();

  GLint transformLoc = glGetUniformLocation(vd.shaderId, "transform");
  glUniformMatrix4fv(transformLoc, 1, GL_FALSE, value_ptr(m));

  int size;
  glActiveTexture(GL_TEXTURE0);	// Activate the texture unit first before binding texture
  glBindTexture(GL_TEXTURE_2D, shp.tx);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // GL_LINEAR);
  PRINT_GL_ERRORS

  auto loc = glGetUniformLocation(shp.shader, "tex0");
  glUniform1i(loc, 0);

  glBindVertexArray(shp.vertary);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shp.idxbuf);                       PRINT_GL_ERRORS
  glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);  PRINT_GL_ERRORS   // todo: keep size with shape instead of querying it
  glDrawElements(shp.mode, shp.indsz, GL_UNSIGNED_INT, 0);                 PRINT_GL_ERRORS   // todo: can't use quads with glDrawElements?
  
  //glDrawElements(GL_QUADS, 4, GL_UNSIGNED_INT, 0);
  
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}
vec_vs         shapesFromKeys(simdb const& db, vec_vs dbKeys, VizData* vd)  // vec<str> const& dbKeys
{
  using namespace std;

  for(auto& vs : dbKeys)
  TO(dbKeys.size(),i)
  {
    auto& vs = dbKeys[i];
    auto cur = vd->shapes.find(vs.str);
    if(cur!=vd->shapes.end() ){
      continue;
    }

    u32     vlen = 0;
    u32  version = 0;
    auto     len = db.len(vs.str.data(), (u32)vs.str.length(), &vlen, &version);          // todo: make ui64 as the input length
    vs.ver = version;

    vec<i8> ivbuf(vlen);
    db.get(vs.str.data(), (u32)vs.str.length(), ivbuf.data(), (u32)ivbuf.size());

    Shape  s      = ivbuf_to_shape(ivbuf.data(), len);      PRINT_GL_ERRORS
    s.shader      = vd->shaderId;
    s.active      = vd->shapes[vs.str].active;
    s.version     = version; //vs.v; // version;
    vd->shapes[vs.str] = move(s);
  };

  return dbKeys;
}
vec_vs       eraseMissingKeys(vec_vs dbKeys, KeyShapes* shps)           // vec<str> dbKeys,
{
  int cnt = 0;
  sort( ALL(dbKeys) );
  //assert( shps->size() == 0 || dbKeys.size() != 0 );
  if( shps->size() > 0 && dbKeys.size() == 0 )
    printf("wat");

  for(auto it = shps->begin(); it != shps->end(); )
  {
    auto const& kv = *it;
    VerStr vs;
    vs.ver = kv.second.version;
    vs.str = kv.first;
    bool isKeyInDb = binary_search(ALL(dbKeys),vs, [](VerStr const& a, VerStr const& b){ return a.str < b.str; } );
    if( !isKeyInDb ){
      it = shps->erase(it);
      ++cnt;
    }else ++it;
  }

  return dbKeys;
  //return cnt;
}
bool                updateKey(simdb const& db, str const& key, u32 version, VizData* vd)
{
  using namespace std;

  u32 dbVersion = 0;
  u32      vlen = 0;
  auto       len = db.len(key.data(), (u32)key.length(), &vlen, &dbVersion);

  if(len>0 && dbVersion!=version){ //key.v){
    vec<i8> ivbuf(vlen);
    db.get(key.data(), (u32)key.length(), ivbuf.data(), (u32)ivbuf.size());

    Shape  s  = ivbuf_to_shape(ivbuf.data(), len);
    s.shader  = vd->shaderId;
    s.active  = true;                     // because updates only happen when the shape is active, always setting an updated shape to active should work
    s.version = dbVersion;
    vd->shapes[key] = move(s);
    
    return true;
  }

  return false;
}
double                   nowd()
{
  using namespace std;
  using namespace std::chrono;

  auto nano = duration_cast<nanoseconds>( chrono::high_resolution_clock::now().time_since_epoch() );

  return nano.count() / 1000000000.0; 
}
void                refreshDB(VizData* vd)
{
  auto dbKeys = db.getKeyStrs();                                      // Get all keys in DB - this will need to be ran in the main loop, but not every frame
  dbKeys      = shapesFromKeys(db, move(dbKeys), vd);
  dbKeys      = eraseMissingKeys(move(dbKeys), &vd->shapes);
  sort(ALL(dbKeys));
  sort(ALL(vd->ui.dbIdxs));                                                  // sort the indices so the largest are removed first and the smaller indices don't change their position
  FROM(vd->ui.dbIdxs.size(), i){ vd->ui.keyWin->removeChild(vd->ui.dbIdxs[i]); }
  vd->ui.dbIdxs.resize(0);
  vd->ui.dbIdxs.shrink_to_fit();
  for(auto key : dbKeys)                                              // add the buttons back and keep track of their indices
  {
    auto b = new Button(vd->ui.keyWin, key.str);
    int  i = vd->ui.keyWin->childIndex(b);
    if(i > -1){ vd->ui.dbIdxs.push_back(i); }
    b->setFlags(Button::ToggleButton);
    b->setChangeCallback([k = key.str](bool pushed){ buttonCallback(k, pushed); });
    b->setPushed(vd->shapes[key.str].active);
    b->setFixedHeight(25);
  }
  vd->ui.screen.performLayout();

  vd->keyRefreshClock -= vd->keyRefresh;
  vd->verRefreshClock -= vd->verRefresh;

  if(vd->keyRefreshClock > vd->keyRefresh){                           // if there is already enough time saved up for another update make sure that less that two updates worth of time is kept 
    vd->keyRefreshClock = vd->keyRefresh + fmod(vd->keyRefreshClock, vd->keyRefresh);
  }
}

}

void       genTestGeo(simdb* db)
{
  using namespace std;
  
  static simdb db1("test 1", 4096, 1 << 14);
  static simdb db2("test 2", 4096, 1 << 14);

  // new (&db2) simdb(name.c_str(), 4096, 1 << 14);             // inititialize the DB with placement new into the data segment

  initSimDB("Viz Default");

  // Create serialized IndexedVerts
  size_t leftLen, rightLen, cubeLen;
  vec<u8>  leftData = makeTriangle(leftLen, true);
  vec<u8> rightData = makeTriangle(rightLen, false);
  vec<u8>  cubeData = makeCube(cubeLen);

  // Store serialized IndexedVerts in the db
  str  leftTriangle = "leftTriangle";
  str rightTriangle = "rightTriangle";
  str          cube = "cube";

  db->put(leftTriangle, leftData);
  db->put(rightTriangle, rightData);
  db->put(cube, cubeData);

  db1.put("1", leftData);
  db1.put("2", rightData);
  db1.put("3", cubeData);

  db2.put("one",    leftData);
  db2.put("two",   rightData);
  db2.put("three",  cubeData);
  db2.put("super long key name as a test", cubeData);

}

ENTRY_DECLARATION
{
  using namespace std;
  using namespace nanogui;

  genTestGeo(&db);

  SECTION(initialization)
  {
    SECTION(static VizData)
    {
      vd.ui.w             = 1024; 
      vd.ui.h             =  768;
      vd.ui.ptSz          =    0.25f;
      vd.ui.hudSz         =   16.0f;

      vd.now              =  nowd();
      vd.prev             =  vd.now;
      vd.verRefresh       =  1.0/144.0;
      vd.verRefreshClock  =  0.0;
      vd.keyRefresh       =  2.0;
      vd.keyRefreshClock  =  vd.keyRefresh;
      vd.camera           =  initCamera();

      vd.avgFps   =  60; 
      vd.prevt    =   0; 
      vd.cpuTime  =   0; 
      vd.t        =   0; 
      vd.dt       =   0;

      vd.ui.keyWin    =  nullptr;
      vd.ui.dbWin     =  nullptr;
      vd.ui.keyLay    =  nullptr;
      vd.ui.dbLst     =  nullptr;
      vd.ui.dbLstIdx  = -1;
      vd.ui.nvg       = nullptr;     
    }
    SECTION(glfw and glew)
    {
      vd.win      = initGLFW( &vd );                        assert(vd.win!=nullptr);
      initGlew();
      vd.shaderId = shadersrc_to_shaderid(vertShader, fragShader);   PRINT_GL_ERRORS

      glfwSetWindowUserPointer(vd.win, &vd);
      glfwSwapInterval(0);
      glfwSwapBuffers(vd.win);
      glfwMakeContextCurrent(vd.win);

      PRINT_GL_ERRORS
    }
    SECTION(nanogui)
    {
      vd.ui.screen.initialize(vd.win, false);

      SECTION(sidebar)
      {
        vd.ui.keyLay     = new BoxLayout(Orientation::Vertical, Alignment::Fill, 2, 5);
        vd.ui.keyWin     = new Window(&vd.ui.screen, "");
        auto spcr1 = new nanogui::Label(vd.ui.keyWin, "");
        auto spcr2 = new nanogui::Label(vd.ui.keyWin, "");
        auto spcr3 = new nanogui::Label(vd.ui.keyWin, "");
        vd.ui.dbLst      = new ComboBox(vd.ui.keyWin, {"None"} );
        vd.ui.dbLst->setChangeCallback( dbLstCallback );
        dbLstCallback(true);
        vd.ui.dbLst->setCallback([](int i){                                          // callback for the actual selection
          if(i < vd.ui.dbNames.size()){
            initSimDB(vd.ui.dbNames[i]);
            refreshDB(&vd);
          }
          vd.ui.screen.performLayout();
        });
        auto spcr4 = new nanogui::Label(vd.ui.keyWin, "");
        auto spcr5 = new nanogui::Label(vd.ui.keyWin, "");
        auto spcr6 = new nanogui::Label(vd.ui.keyWin, "");

        vd.ui.dbLstIdx = vd.ui.keyWin->childIndex(vd.ui.dbLst);                                  // only done once so not a problem even though it is a linear search
        vd.ui.keyWin->setLayout(vd.ui.keyLay);
        vd.ui.dbLst->setSide(Popup::Left);

        Theme* thm = vd.ui.keyWin->theme();
        thm->mTransparent         = v4f( .0f,  .0f,  .0f,    .0f );
        thm->mWindowFillUnfocused = v4f( .2f,  .2f,  .225f,  .3f );
        thm->mWindowFillFocused   = v4f( .3f,  .28f, .275f,  .3f );
      }

      vd.ui.screen.setVisible(true);
      vd.ui.screen.performLayout();
    }
    SECTION(nanovg)
    {
      vd.ui.nvg =  nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
      if(vd.ui.nvg == NULL){
        printf("Could not init nanovg.\n");
        return -1;
      }
                 nvgCreateFont(vd.ui.nvg, "sans",      "Roboto-Regular.ttf" );
      int font = nvgCreateFont(vd.ui.nvg, "sans-bold", "Roboto-Bold.ttf"    );
      if(font == -1){
        printf("Could not add font bold.\n");
        return -1;
      }
    }
  }

  while(!glfwWindowShouldClose(vd.win))
  {
    PRINT_GL_ERRORS

    SECTION(time)
    {
      vd.now  = nowd();
      double passed = vd.now - vd.prev;
      vd.prev = vd.now;
      vd.keyRefreshClock += passed;
      vd.verRefreshClock += passed;

      vd.t     = glfwGetTime();
      vd.dt    = vd.t - vd.prevt;
      vd.prevt = vd.t;

    }
    SECTION(database)
    {
      if(vd.keyRefreshClock > vd.keyRefresh){
        glfwPollEvents();
        refreshDB(&vd);
      } // end of updates to shapes 
      PRINT_GL_ERRORS
    }
    SECTION(input)
    {
      vd.camera.mouseDelta = vec2(0,0);
      vd.camera.btn2Delta  = vec2(0,0);
      glfwPollEvents();                                             // PollEvents must be done after zeroing out the deltas
      glfwGetWindowSize(vd.win, &vd.ui.w, &vd.ui.h);

      PRINT_GL_ERRORS
    }
    SECTION(OpenGL setup)
    {
      glViewport(0, 0, vd.ui.w, vd.ui.h);

      //glEnable(GL_TEXTURE_2D);
      //glEnable(GL_DEPTH);
      glEnable(GL_DEPTH_TEST);                                   // glDepthFunc(GL_LESS);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      PRINT_GL_ERRORS

      glEnable(GL_BLEND);
      glLineWidth(vd.ui.ptSz);
      glPointSize(vd.ui.ptSz);

      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glClearColor(0.0625, 0.0625, 0.0625, 0.0625);
      PRINT_GL_ERRORS
    }
    SECTION(render shapes)
    {
      const static auto XAXIS = vec3(1.f, 0.f, 0.f);
      const static auto YAXIS = vec3(0.f, 1.f, 0.f);

      mat4  viewProj;
      vec3      camx;    //  =  normalize( vec3(cam.pantfm*vec4(1.f,0,0,1.f)) );              // use a separate transform for panning that is frozen on mouse down so that the panning space won't constantly be changing due to rotation from the lookat function
      vec3      camy;    //  =  normalize( vec3(cam.pantfm*vec4(0,1.f,0,1.f)) );              // why does the transform change though if both the lookat and offset are being translated? are they not translated at the same time?
      SECTION(update camera)
      {
        auto&  cam    =  vd.camera;
        f32    dst    =  cam.dist;

        float   ry    =  -(cam.mouseDelta.x * cam.sensitivity);
        float   rx    =  -(cam.mouseDelta.y * cam.sensitivity);

        mat4    xzmat = rotate(mat4(), ry, YAXIS);
        mat4     ymat = rotate(mat4(), rx, XAXIS);
        vec4        p = vec4(cam.pos, 1.f);
        cam.pos       =  xzmat * p;
        vec4     ypos(0, p.y, dst, 1.f);
        cam.pos.y     = (ymat * ypos).y;

        vec3 lookOfst = normalize(cam.pos-cam.lookAt)*dst;
        cam.pos = cam.lookAt + lookOfst;
        
        camx   =  normalize( vec3(cam.pantfm*vec4(1.f,0,0,1.f)) );              // use a separate transform for panning that is frozen on mouse down so that the panning space won't constantly be changing due to rotation from the lookat function
        camy   =  normalize( vec3(cam.pantfm*vec4(0,1.f,0,1.f)) );              // why does the transform change though, if both the lookat and offset are being translated? are they not translated at the same time?

        vec3  ofst(0,0,0); 

        ofst        +=  camx *  -cam.btn2Delta.x * cam.pansense * dst;
        ofst        +=  camy *   cam.btn2Delta.y * cam.pansense * dst;
        cam.lookAt  +=  ofst;
        cam.pos     +=  ofst;
        
        viewProj     =  camera_to_mat4(cam, (f32)vd.ui.w, (f32)vd.ui.h);
      }
      SECTION(draw shapes)
      {
        //for(auto& kv : vd.shapes){
        //  kv.second.active = true;
        //}

        for(auto& kv : vd.shapes){
          if(kv.second.active)
            RenderShape(kv.second, viewProj);
        }
      }

      PRINT_GL_ERRORS
    }
    SECTION(nanogui)
    {
      if(vd.ui.keyWin->focused()){
        vd.camera.leftButtonDown  = false;
        vd.camera.rightButtonDown = false;
      }

      v2i   winsz = vd.ui.keyWin->size();
      v2i keyspos = v2i(vd.ui.screen.width() - winsz.x(), 0);
      vd.ui.keyWin->setPosition(keyspos);
      vd.ui.keyWin->setSize(v2i(winsz.x(), vd.ui.screen.height()));

      vd.ui.screen.drawContents();
      vd.ui.screen.drawWidgets();
    }
    SECTION(nanovg | frames per second and color under cursor) 
    {
      nvgBeginFrame(vd.ui.nvg, vd.ui.w, vd.ui.h, vd.ui.w/(f32)vd.ui.h);
        vd.avgFps *= 0.9;
        vd.avgFps += (1.0 / vd.dt)*0.1;
        //int fps = (int)avgFps;

        char str[TITLE_MAX_LEN];
        sprintf(str, "%04.01f", vd.avgFps);

        f32 tb = nvgTextBounds(vd.ui.nvg, 0, 0, str, NULL, NULL);
        nvgFontSize(vd.ui.nvg, vd.ui.hudSz);
        //nvgFontFace(nvg, "sans-bold");
        nvgFontFace(vd.ui.nvg, "sans");
        nvgTextAlign(vd.ui.nvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);  // NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
        nvgFillColor(vd.ui.nvg, nvgRGBA(255, 255, 255, 255));
        nvgText(vd.ui.nvg, 15.f, vd.ui.hudSz, str, NULL);


        auto rgb = vd.mouseRGB;
        sprintf(str, "%.2f  %.2f  %.2f", rgb[0], rgb[1], rgb[2]);
        //f32 rgbBnds = nvgTextBounds(nvg, tb, 0, str, NULL, NULL);
        nvgFontSize(vd.ui.nvg, vd.ui.hudSz);
        //nvgFontFace(nvg, "sans-bold");
        nvgFontFace(vd.ui.nvg, "sans");
        nvgTextAlign(vd.ui.nvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vd.ui.nvg, nvgRGBA(255, 255, 255, 255));
        nvgText(vd.ui.nvg, tb + 45.f, vd.ui.hudSz, str, NULL);

      nvgEndFrame(vd.ui.nvg);
    }

    glfwSwapBuffers(vd.win);
    PRINT_GL_ERRORS

    glReadPixels( (GLint)(vd.camera.oldMousePos.x), (GLint)(vd.camera.oldMousePos.y), 1, 1, GL_RGB, GL_FLOAT, vd.mouseRGB);
  }

  nanogui::shutdown();
  glfwTerminate();

  //int a; cin >> a;

  return 0;
}

