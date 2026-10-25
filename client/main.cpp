// Browser renderer adapted from Treasure Labyrinth's UIMaze texture mapping and Camera2D.
// The FastAPI server owns positions, visibility, collisions, questions and score.
#include <raylib.h>
#include <emscripten/emscripten.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

constexpr int MAX_CELLS = 4096, MAX_PLAYERS = 64;
constexpr float TILE = 96;
struct Pawn { int id=0, cell=0, role=0, self=0; bool visible=false, initialized=false;
              int protectedFlag=0, busy=0; Vector2 drawn{}; };
struct Station { int cell, solved; };
int width=18, height=14, focus=0, host=0, stationCount=0;
int paths[MAX_CELLS];
Texture2D textures[16]{};
Texture2D warrior{};
Pawn pawns[MAX_PLAYERS];
Station stations[16];
Camera2D camera{};
bool cameraReady=false;

extern "C" {
EMSCRIPTEN_KEEPALIVE void hp_resize(int w, int h) {
  SetWindowSize(std::max(1, w), std::max(1, h));
}
EMSCRIPTEN_KEEPALIVE void hp_map(int w, int h) {
  if (w*h > MAX_CELLS || w < 1 || h < 1) return;
  if (width != w || height != h) cameraReady=false;
  width=w; height=h;
}
EMSCRIPTEN_KEEPALIVE void hp_tile(int i, int mask) {
  if (i >= 0 && i < MAX_CELLS) paths[i]=mask;
}
EMSCRIPTEN_KEEPALIVE void hp_players_clear() {
  for (auto& p : pawns) p.visible=false;
}
EMSCRIPTEN_KEEPALIVE void hp_player(int id, int cell, int role, int self, int shield, int busy) {
  Pawn* target=nullptr;
  for (auto& p : pawns) if (p.initialized && p.id == id) { target=&p; break; }
  if (!target) for (auto& p : pawns) if (!p.initialized) { target=&p; break; }
  if (!target) return;
  if (!target->initialized) {
    target->drawn={(cell%width+0.5f)*TILE, (cell/width+0.5f)*TILE};
    target->initialized=true;
  }
  target->id=id; target->cell=cell; target->role=role; target->self=self;
  target->protectedFlag=shield; target->busy=busy; target->visible=true;
}
EMSCRIPTEN_KEEPALIVE void hp_focus(int cell, int fullMap) { focus=cell; host=fullMap; }
EMSCRIPTEN_KEEPALIVE void hp_stations_clear() { stationCount=0; }
EMSCRIPTEN_KEEPALIVE void hp_station(int cell, int solved) {
  if (stationCount < 16) stations[stationCount++]={cell,solved};
}
}

void frame() {
  float dt=std::min(GetFrameTime(), 0.1f);
  camera.offset={GetScreenWidth()/2.0f, GetScreenHeight()/2.0f};
  camera.zoom=host ? std::min(GetScreenWidth()/(width*TILE+70), GetScreenHeight()/(height*TILE+70))
                   : std::min(GetScreenWidth(), GetScreenHeight())/(TILE*7.8f);
  const Vector2 target=host ? Vector2{width*TILE/2, height*TILE/2}
                           : Vector2{(focus%width+0.5f)*TILE, (focus/width+0.5f)*TILE};
  if (!cameraReady || std::hypot(target.x-camera.target.x,target.y-camera.target.y)>TILE*4) {
    camera.target=target; cameraReady=true;
  }
  camera.target.x+=(target.x-camera.target.x)*std::min(1.0f,dt*12);
  camera.target.y+=(target.y-camera.target.y)*std::min(1.0f,dt*12);
  BeginDrawing();
  ClearBackground(Color{17,22,23,255});
  BeginMode2D(camera);
  for (int i=0;i<width*height;++i) {
    Rectangle rect{(i%width)*TILE,(i/width)*TILE,TILE,TILE};
    if (paths[i]<0) {
      DrawRectangleRec(rect,Color{22,28,29,255});
      DrawRectangleLinesEx(rect,1,Color{26,33,33,255});
      continue;
    }
    const int mask=paths[i]&15;
    if (textures[mask].id) {
      DrawTexturePro(textures[mask],Rectangle{0,0,(float)textures[mask].width,(float)textures[mask].height},
                     rect,Vector2{0,0},0,Color{185,191,171,255});
    } else DrawRectangleRec(rect,Color{67,79,69,255});
    Color wall{15,24,24,245};
    if (!(mask&1)) DrawRectangleRec(Rectangle{rect.x,rect.y,TILE,5},wall);
    if (!(mask&2)) DrawRectangleRec(Rectangle{rect.x+TILE-5,rect.y,5,TILE},wall);
    if (!(mask&4)) DrawRectangleRec(Rectangle{rect.x,rect.y+TILE-5,TILE,5},wall);
    if (!(mask&8)) DrawRectangleRec(Rectangle{rect.x,rect.y,5,TILE},wall);
  }
  for (int i=0;i<stationCount;++i) {
    Station s=stations[i];
    Vector2 pos{(s.cell%width+0.5f)*TILE,(s.cell/width+0.5f)*TILE};
    Color ink=s.solved ? Color{118,144,123,255} : Color{245,204,111,255};
    DrawCircleV(pos,28,Fade(ink,0.12f));
    DrawPoly(pos,4,19,45,Color{29,38,34,255});
    DrawPolyLinesEx(pos,4,19,45,2,ink);
    DrawText(s.solved?"+":"?",(int)pos.x-5,(int)pos.y-9,20,ink);
  }
  for (auto& p : pawns) {
    if (!p.visible) continue;
    Vector2 position{(p.cell%width+0.5f)*TILE,(p.cell/width+0.5f)*TILE};
    if (std::hypot(position.x-p.drawn.x,position.y-p.drawn.y)>TILE*4) p.drawn=position;
    p.drawn.x+=(position.x-p.drawn.x)*std::min(1.0f,dt*16);
    p.drawn.y+=(position.y-p.drawn.y)*std::min(1.0f,dt*16);
    Color ink=p.role ? Color{240,110,83,255} : p.self ? Color{158,224,183,255}
                                                    : Color{161,187,221,255};
    if (p.role) {
      const float pulse=0.10f+0.04f*std::sin((float)GetTime()*3);
      DrawCircleV(p.drawn,58,Fade(ink,pulse));
      DrawCircleV(p.drawn,38,Fade(ink,pulse*2));
    }
    DrawEllipse((int)p.drawn.x,(int)p.drawn.y+15,19,9,Fade(BLACK,0.5f));
    DrawCircleV(p.drawn,20,Color{27,35,32,255});
    DrawCircleLinesV(p.drawn,22,ink);
    if (warrior.id) DrawTexturePro(warrior,Rectangle{0,0,(float)warrior.width,(float)warrior.height},
      Rectangle{p.drawn.x-15,p.drawn.y-23,30,36},Vector2{0,0},0,ink);
    else DrawCircleV(p.drawn,12,ink);
    if (p.protectedFlag) DrawCircleLinesV(p.drawn,28,Color{158,224,183,190});
    if (p.role || p.self) {
      const char* label=p.role?"PROFE":"TU";
      int tw=MeasureText(label,14);
      DrawRectangle((int)p.drawn.x-tw/2-5,(int)p.drawn.y-43,tw+10,20,Color{19,26,24,230});
      DrawText(label,(int)p.drawn.x-tw/2,(int)p.drawn.y-40,14,ink);
    }
    if (p.busy) DrawText("...",(int)p.drawn.x+18,(int)p.drawn.y-28,20,WHITE);
  }
  EndMode2D();
  EndDrawing();
}

int main() {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(960,640,"Hipotesis Party");
  SetTargetFPS(60);
  for (auto& mask : paths) mask=-1;
  // Exact OpenPaths -> texture correspondence from the original UIMaze::assignTileTexture.
  const char* names[16]={nullptr,"Wall1N","Wall1E","Wall2NE","Wall1S","Wall2NS","Wall2SE",
    "Wall3NSE","Wall1W","Wall2NW","Wall2EW","Wall3NEW","Wall2SW","Wall3NSW","Wall3SEW","Wall4Cross"};
  for (int mask=1;mask<16;++mask) {
    char path[160];
    std::snprintf(path,sizeof(path),"/assets/TreasureLabirynthFloor/%s.png",names[mask]);
    textures[mask]=LoadTexture(path);
    SetTextureFilter(textures[mask],TEXTURE_FILTER_POINT);
  }
  warrior=LoadTexture("/assets/WarriorOutfit.png");
  emscripten_set_main_loop(frame,0,1);
}
