#include "PartyGame.h"

#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <sstream>
#include <utility>
#include <chrono>
#include <cstdlib>
#include <queue>
#include <cctype>

namespace hp {
namespace {
constexpr float Tile = 3.0f;
constexpr std::array<const char*, 6> Characters{
    "student_f01", "student_f02", "student_f03", "student_m01", "student_m02", "student_m03"};
constexpr std::array<Color, 6> GlyphColors{{{87,230,207,255},{235,190,110,255},{153,159,255,255},{105,191,255,255},{239,143,167,255},{193,225,140,255}}};
Cell ReadCell(const nlohmann::json& j) { return {j.value("x",0),j.value("z",0)}; }
nlohmann::json WriteCell(Cell c) { return {{"x",c.x},{"z",c.z}}; }
bool ReadNumber(std::string value,double& result) {
  std::replace(value.begin(),value.end(),',','.');
  if (value.empty()) return false;
  char* end=nullptr; result=std::strtod(value.c_str(),&end);
  return end==value.c_str()+value.size() && std::isfinite(result);
}
class CallbackScene final : public Scene {
 public:
  CallbackScene(std::function<void(float)> update, std::function<void()> draw,
                std::function<void()> enter = {}) : update_(std::move(update)),draw_(std::move(draw)),enter_(std::move(enter)) {}
  void Enter() override { ui::ResetInput(); if (enter_) enter_(); }
  void Update(float dt) override { update_(dt); }
  void Draw() override { draw_(); }
 private:
  std::function<void(float)> update_;
  std::function<void()> draw_, enter_;
};
Color Alpha(Color color, float alpha) { return Fade(color,alpha); }
}

PartyGame::~PartyGame() {
  ui::ResetFont();
  resources_.UnloadAll();
  if (shaderReady_) UnloadShader(lighting_);
}

void PartyGame::QueueAssets() {
  if (FileExists((assetRoot_+"fonts/interface.ttf").c_str()))
    resources_.QueueFont("interface",assetRoot_+"fonts/interface.ttf",48);
  if (FileExists((assetRoot_+"fonts/heading.ttf").c_str()))
    resources_.QueueFont("heading",assetRoot_+"fonts/heading.ttf",80);
  resources_.QueueModel("professor",assetRoot_+"models/professor.glb");
  resources_.QueueModelAnimations("professor",assetRoot_+"models/professor.glb");
  for (const char* id : Characters) {
    resources_.QueueModel(id,assetRoot_+"models/"+id+".glb");
    resources_.QueueModelAnimations(id,assetRoot_+"models/"+id+".glb");
  }
  for (const char* id : {"floor","wall","podium","pillar","glyph"})
    resources_.QueueModel(id,assetRoot_+"models/"+id+".glb");
}

void PartyGame::PrepareMaterials() {
  if (shaderReady_) return;
#ifdef __EMSCRIPTEN__
  const std::string suffix = "_web";
#else
  const std::string suffix;
#endif
  lighting_ = LoadShader((assetRoot_+"shaders/lighting"+suffix+".vs").c_str(),
                         (assetRoot_+"shaders/lighting"+suffix+".fs").c_str());
  shaderReady_ = IsShaderValid(lighting_) && lighting_.id != rlGetShaderIdDefault();
  if (shaderReady_) {
    lighting_.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(lighting_,"matModel");
    lighting_.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(lighting_,"matNormal");
    lighting_.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(lighting_,"viewPos");
    auto apply = [this](const char* id) {
      if (auto* model = resources_.GetModel(id))
        for (int i=0;i<model->materialCount;++i) model->materials[i].shader = lighting_;
    };
    for (const auto* id : Characters) apply(id);
    for (const auto* id : {"professor","floor","wall","podium","pillar"}) apply(id);
    // Glyphs use the unlit default shader so their color remains luminous.
  }
  if (auto* font = resources_.GetFont("interface")) ui::SetFont(*font);
  if (auto* font = resources_.GetFont("heading")) ui::SetHeadingFont(*font);
}

void PartyGame::RegisterScenes() {
  scenes_.Register(SceneId::Loading,std::make_unique<CallbackScene>([this](float dt){UpdateLoading(dt);},[this]{DrawLoading();}));
  scenes_.Register(SceneId::Menu,std::make_unique<CallbackScene>([this](float dt){UpdateMenu(dt);},[this]{DrawMenu();},[this]{status_.clear();}));
  scenes_.Register(SceneId::Exploration,std::make_unique<CallbackScene>([this](float dt){UpdateExploration(dt);},[this]{DrawExploration();}));
  scenes_.Register(SceneId::Results,std::make_unique<CallbackScene>([this](float dt){UpdateResults(dt);},[this]{DrawResults();}));
}

Rectangle PartyGame::Rect(float x,float y,float w,float h) const { return ui::Canvas(1440,900).ToScreen({x,y,w,h}); }

void PartyGame::Text(const std::string& text,float x,float y,float size,Color color) const {
  const ui::Canvas canvas(1440,900); const auto p=canvas.ToScreen(Vector2{x,y});
  if (size>=30) ui::Title(text,p.x,p.y,canvas.Scale(size),color);
  else ui::Text(text,p.x,p.y,canvas.Scale(size),color);
}

bool PartyGame::Button(float x,float y,float w,float h,const std::string& label,bool accent,bool enabled) {
  return ui::Button(Rect(x,y,w,h),label,accent,enabled);
}

Vector3 PartyGame::World(Cell cell) const { return {cell.x*Tile,0,cell.z*Tile}; }

std::vector<PartyGame::AvatarVisual> PartyGame::VisibleAvatars() const {
  std::vector<AvatarVisual> avatars;
  // A null cell is an unseen participant. Never infer their position from the roster.
  for(const auto& team:state_.value("teams",Json::array())) {
    if(!team.contains("cell")||!team["cell"].is_object())continue;
    const int id=team.value("id",0);
    const Cell cell=ReadCell(team["cell"]);
    const bool own=!IsTeacher()&&id==teamId_;
    avatars.push_back({id,Characters[std::clamp(team.value("avatarIndex",0),0,5)],
      team.value("avatarName",""),cell,own?position_:World(cell),GlyphColors[std::clamp(id,0,5)],own});
  }
  if(state_.contains("teacher")&&state_["teacher"].is_object()) {
    const Cell cell=ReadCell(state_["teacher"]);
    avatars.push_back({-1,"professor",state_.value("teacherName","Profesora"),cell,
      IsTeacher()?position_:World(cell),ui::Gold,IsTeacher()});
  }
  std::sort(avatars.begin(),avatars.end(),[](const auto& a,const auto& b){return a.id<b.id;});
  for(auto& avatar:avatars) {
    int count=0,slot=0;
    for(const auto& other:avatars)if(other.cell==avatar.cell) {
      if(other.id<avatar.id)++slot;
      ++count;
    }
    if(count>1) {
      const float angle=-PI*.5f+2*PI*slot/count;
      const float radius=count==2?.62f:.88f;
      avatar.at.x+=std::cos(angle)*radius;
      avatar.at.z+=std::sin(angle)*radius;
    }
  }
  return avatars;
}

PartyGame::Json PartyGame::RankedTeams() const {
  auto teams=state_.value("teams",Json::array());
  std::sort(teams.begin(),teams.end(),[](const auto& a,const auto& b){
    const int aScore=a.value("score",0),bScore=b.value("score",0);
    return aScore!=bScore?aScore>bScore:a.value("id",0)<b.value("id",0);
  });
  return teams;
}

void PartyGame::UpdateLoading(float) {
  if (!resources_.IsLoadingComplete()) {
    resources_.UpdateLoading(1);
    if (auto* font=resources_.GetFont("interface")) {
      SetTextureFilter(font->texture,TEXTURE_FILTER_BILINEAR); ui::SetFont(*font);
    }
    if (auto* font=resources_.GetFont("heading")) {
      SetTextureFilter(font->texture,TEXTURE_FILTER_BILINEAR); ui::SetHeadingFont(*font);
    }
  }
  else {
    PrepareMaterials();
    if (IsKeyPressed(KEY_ENTER) && resources_.GetFailedResources().empty()) scenes_.Request(SceneId::Menu);
  }
}

void PartyGame::DrawChrome(const char* section) {
  DrawRectangleRec(Rect(0,0,1440,93),ui::Navy);
  Text("H / P",34,26,30,ui::Gold);
  Text("HIPÓTESIS PARTY",142,22,23);
  Text("CI-0115  /  EL LABERINTO DE LAS IDEAS",143,53,14,ui::Muted);
  Text(section,1082,34,18,ui::Gold);
  DrawRectangleRec(Rect(32,92,1376,1),ui::Line);
}

void PartyGame::DrawLoading() {
  DrawChrome("PREPARANDO EL CAMPUS");
  for (int i=0;i<8;++i) {
    const float radius=180+i*32;
    const auto center=ui::Canvas(1440,900).ToScreen(Vector2{1090,440});
    DrawCircleLinesV(center,ui::Canvas(1440,900).Scale(radius),Alpha(ui::Teal,0.03f+(i%3)*0.02f));
  }
  Text("UN MUNDO DE",84,172,23,ui::Teal);
  Text("ideas por descubrir.",80,210,57);
  Text("Preparando personajes, animaciones y el campus 3D.",84,291,21,ui::Muted);
  const float progress=resources_.GetLoadingProgress();
  Text(TextFormat("%02d%%",static_cast<int>(progress*100)),83,375,74,ui::Gold);
  ui::ProgressBar(Rect(84,470,780,10),progress);
  Text(TextFormat("%zu de %zu recursos procesados",resources_.GetProcessedResourceCount(),resources_.GetTotalResourceCount()),84,500,19);
  const bool complete=resources_.IsLoadingComplete();
  Text(complete?"Carga finalizada":("Siguiente: "+resources_.GetNextResourceName()),84,535,18,ui::Muted);
  ui::Panel(Rect(938,173,418,554));
  Text("REGISTRO DE ASSETS",968,205,19,ui::Gold);
  const auto& history=resources_.GetLoadHistory();
  const auto begin=history.size()>11?history.size()-11:0;
  for (std::size_t i=begin;i<history.size();++i) {
    const auto& entry=history[i]; const float y=255+(i-begin)*36;
    Text(entry.success?"OK":"!",969,y,16,entry.success?ui::Teal:ui::Error);
    Text(entry.name,1013,y,18,entry.success?ui::Cream:ui::Error);
  }
  if (complete && resources_.GetFailedResources().empty()) {
    if (Button(84,618,340,58,"Entrar al campus  [Enter]",true)) scenes_.Request(SceneId::Menu);
    Text("Siete avatares. Pistas exclusivas. Una hipótesis por equipo.",84,704,18,ui::Muted);
  } else if (complete) {
    Text("No se pudieron cargar algunos archivos.",84,598,22,ui::Error);
    Text("Revisa assets/models o vuelve a generar los modelos.",84,634,18,ui::Muted);
    if (Button(84,685,270,52,"Reintentar carga",true)) {
      ui::ResetFont(); resources_.UnloadAll(); QueueAssets();
      if (shaderReady_) { UnloadShader(lighting_); shaderReady_=false; }
    }
  }
  Text("MODELOS ORIGINALES  /  BLENDER + RAYLIB",84,827,16,ui::Muted);
}

void PartyGame::DrawProp(const char* id,Vector3 at,Vector3 scale,float rotation,Color tint) {
  if (auto* model=resources_.GetModel(id)) DrawModelEx(*model,at,{0,1,0},rotation,scale,tint);
}

void PartyGame::DrawCharacter(const std::string& id,Vector3 at,float heading,const char* clip,float clock,float scale) {
  auto* model=resources_.GetModel(id); const auto* animations=resources_.GetModelAnimations(id);
  if (!model) return;
  if (animations) for (int i=0;i<animations->count;++i) {
    const auto& animation=animations->animations[i];
    if (std::strcmp(animation.name,clip)==0 && animation.keyframeCount>0 && IsModelAnimationValid(*model,animation)) {
      UpdateModelAnimation(*model,animation,std::fmod(clock*60.0f,static_cast<float>(animation.keyframeCount)));
      break;
    }
  }
  // A soft contact shadow grounds the characters without an expensive shadow map.
  DrawCylinder({at.x,at.y+0.014f,at.z},0.34f*scale,0.34f*scale,0.006f,24,Alpha(BLACK,0.17f));
  DrawModelEx(*model,at,{0,1,0},heading,{scale,scale,scale},WHITE);
}

void PartyGame::DrawGlyph(Vector3 pos,int id,float scale) {
  const Color color=GlyphColors[id%6];
  pos.y+=1.45f+std::sin(time_*1.8f+id)*0.16f;
  DrawProp("glyph",pos,{scale,scale,scale},time_*38+id*60,color);
  rlDrawRenderBatchActive();
  rlDisableDepthMask();
  BeginBlendMode(BLEND_ADDITIVE);
  DrawSphereEx(pos,0.37f*scale,8,12,Alpha(color,0.10f));
  DrawSphereEx(pos,0.49f*scale,8,12,Alpha(color,0.035f));
  EndBlendMode();
  rlDrawRenderBatchActive();
  rlEnableDepthMask();
  DrawCircle3D({pos.x,0.05f,pos.z},0.62f*scale,{1,0,0},90,Alpha(color,0.65f));
  for (int p=0;p<7;++p) {
    const float phase=time_*0.7f+p*0.90f+id;
    const Vector3 mote={pos.x+std::cos(phase)*0.55f*scale,pos.y+std::sin(phase*1.7f)*0.65f*scale,pos.z+std::sin(phase)*0.55f*scale};
    DrawSphereEx(mote,0.023f*scale,3,4,Alpha(color,0.85f));
  }
}

void PartyGame::DrawTextWrapped(const std::string& text,Rectangle area,float size,Color color) {
  const float scale=ui::Canvas(1440,900).GetScale();
  size*=scale;
  std::istringstream words(text); std::string word,line; float y=area.y;
  while (words>>word) {
    const auto candidate=line.empty()?word:line+" "+word;
    if (!line.empty() && ui::Measure(candidate,size).x>area.width) {
      ui::Text(line,area.x,y,size,color); y+=size*1.5f; line=word;
    } else line=candidate;
  }
  if (!line.empty()) ui::Text(line,area.x,y,size,color);
}

bool PartyGame::ValidateAssets() {
  if (!resources_.GetFailedResources().empty() || !shaderReady_) return false;
  auto valid=[this](const char* id) {
    const auto* model=resources_.GetModel(id); const auto* clips=resources_.GetModelAnimations(id);
    if (!model || !clips || clips->count<3 || model->skeleton.boneCount==0) return false;
    for (const char* name:{"Idle","Walk","Celebrate"}) {
      bool found=false;
      for (int i=0;i<clips->count;++i) if (std::strcmp(clips->animations[i].name,name)==0)
        found=clips->animations[i].keyframeCount>1 && IsModelAnimationValid(*model,clips->animations[i]);
      if (!found) return false;
    }
    return true;
  };
  if (!valid("professor")) return false;
  for (auto id:Characters) if (!valid(id)) return false;
  return true;
}


PartyGame::PartyGame(std::uint32_t seed,bool smokeTest):seed_(seed),smoke_(smokeTest) {
#ifdef __EMSCRIPTEN__
  assetRoot_="assets/";
#else
  if (DirectoryExists("assets/models")) assetRoot_="assets/";
  else if (DirectoryExists(TextFormat("%sassets/models",GetApplicationDirectory()))) assetRoot_=std::string(GetApplicationDirectory())+"assets/";
  else assetRoot_=std::string(HIPOTESIS_ASSET_DIR)+"/";
#endif
  requestEpoch_=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  camera_.up={0,1,0};camera_.fovy=42;camera_.projection=CAMERA_PERSPECTIVE;
  camera_.position={10,8,15.5f};camera_.target={-.8f,1.3f,0};
  known_.assign(169,-1);visible_.assign(169,false);
  try {
    const auto saved=Json::parse(client_.RestoreSession(),nullptr,false);
    if(saved.is_object()) {
    code_=saved.value("code","");token_=saved.value("token","");role_=saved.value("role","");teamId_=saved.value("teamId",-1);
    }
  } catch (...) {}
  QueueAssets();RegisterScenes();scenes_.Request(SceneId::Loading);
}
std::string PartyGame::Phase() const { return state_.value("phase","lobby"); }
const PartyGame::Json& PartyGame::Own() const {
  static const Json empty=Json::object();
  const auto it=state_.find("own");return it!=state_.end()&&it->is_object()?*it:empty;
}
const PartyGame::Json* PartyGame::MyTeam() const {
  if (!state_.contains("teams")) return nullptr;
  for (const auto& team:state_["teams"]) if (team.value("id",-1)==teamId_) return &team;
  return nullptr;
}
std::string PartyGame::AvatarId() const {
  if (IsTeacher()) return "professor";
  const auto* team=MyTeam();return Characters[team?std::clamp(team->value("avatarIndex",0),0,5):selected_];
}
const PartyGame::Json& PartyGame::ActiveQuestion() const {
  static const Json empty=Json::object();
  if (FeedbackOpen()) return lastQuestion_;
  const auto it=state_.find("question");return it!=state_.end()&&it->is_object()?*it:empty;
}
bool PartyGame::FeedbackOpen() const {
  const auto it=state_.find("feedback");
  return !IsTeacher() && it!=state_.end() && it->is_object() && it->value("questionId",-1)!=dismissedFeedback_;
}
bool PartyGame::QuestionOpen() const {
  return !IsTeacher() && (FeedbackOpen() || (state_.contains("question")&&state_["question"].is_object()));
}
void PartyGame::Enqueue(Request request) { requests_.push_back(std::move(request)); }
void PartyGame::SendAction(const std::string& action,Json extra) {
  if (token_.empty()||actionPending_) return;
  extra["action"]=action;extra["code"]=code_;extra["token"]=token_;
  extra["requestId"]=std::to_string(requestEpoch_)+"-"+std::to_string(++sequence_);
  actionPending_=true;Enqueue({"POST","/api/action",std::move(extra),true});
}
void PartyGame::JoinRoom() {
  if (client_.Busy()||actionPending_||!requests_.empty()) return;
  if (avatarName_.empty()) {status_="Escribe el nombre de tu avatar.";focus_=1;return;}
  Json body={{"avatarName",avatarName_},{"requestId",SessionClient::NewId()}};
  if (teacherChoice_) {body["name"]=avatarName_;if(seed_!=0||smoke_)body["seed"]=seed_;}
  else {
    if (teamName_.empty()||joinCode_.size()!=6) {status_="Escribe el equipo y el código de seis caracteres.";return;}
    body["teamName"]=teamName_;body["code"]=joinCode_;body["avatarIndex"]=selected_;
  }
  status_.clear();actionPending_=true;
  Enqueue({"POST",teacherChoice_?"/api/rooms":"/api/join",std::move(body),true});
}
void PartyGame::ResetAccess() {
  token_.clear();code_.clear();role_.clear();teamId_=-1;state_=Json::object();
  route_.clear();moving_=false;havePosition_=false;client_.SaveSession("");
  known_.assign(169,-1);visible_.assign(169,false);dismissedFeedback_=-1;
  scenes_.Request(SceneId::Menu);
}
void PartyGame::UpdateNetwork(float dt) {
  pollTimer_-=dt;
  if (auto reply=client_.Poll()) {
    try {
      const auto data=Json::parse(*reply,nullptr,false);
      if(!data.is_object())throw std::runtime_error("Unexpected response");
      if (!data.value("ok",false)) {
        const auto error=data.value("error",Json::object());
        status_=error.value("message","La partida no pudo procesar la solicitud.");
        const auto code=error.value("code","");networkError_=code=="connection";
        if (networkError_ && inFlight_ && inFlight_->action) {
          // Same idempotency key on a transport retry, never duplicate an award.
          requests_.push_front(*inFlight_);pollTimer_=1.5f;
        } else {
          actionPending_=false;route_.clear();
          if (code=="unauthorized"||code=="room_not_found"||code=="invalid_token"||code=="room_unavailable") ResetAccess();
        }
      } else {
        networkError_=false;status_.clear();
        if (data.contains("token")) {
          code_=data.value("code","");token_=data.value("token","");role_=data.value("role","");teamId_=data.value("teamId",-1);
          client_.SaveSession(Json{{"code",code_},{"token",token_},{"role",role_},{"teamId",teamId_}}.dump());
        }
        if (data.contains("state")) ApplyState(data["state"]);
        if (inFlight_&&inFlight_->action) actionPending_=false;
      }
    } catch (...) {status_="El servidor devolvió una respuesta inesperada.";networkError_=true;actionPending_=false;}
    inFlight_.reset();if (!networkError_) pollTimer_=.35f;
  }
  if (client_.Busy()) return;
  if (!requests_.empty() && (!networkError_||pollTimer_<=0)) {
    inFlight_=std::move(requests_.front());requests_.pop_front();
    client_.Send(inFlight_->method,inFlight_->path,inFlight_->body.dump());
  } else if (!token_.empty() && requests_.empty() && pollTimer_<=0) {
    inFlight_=Request{"GET","/api/state?code="+code_+"&token="+token_,Json::object(),false};
    client_.Send("GET",inFlight_->path);pollTimer_=.35f;
  }
}
void PartyGame::ApplyState(const Json& incoming) {
  if (!incoming.is_object()) return;
  if(!role_.empty()&&incoming.value("role","")!=role_)return;
  if(incoming.value("roomCode","")==state_.value("roomCode","") && incoming.value("role","")==state_.value("role","") && incoming.value("revision",0)<state_.value("revision",0))return;
  const int previousClues=Own().value("clues",Json::array()).size();
  state_=incoming;role_=state_.value("role",role_);teamId_=state_.value("teamId",teamId_);
  width_=state_.value("width",13);height_=state_.value("height",13);
  known_.assign(width_*height_,-1);visible_.assign(width_*height_,false);
  for (const auto& c:state_.value("cells",Json::array())) {
    const Cell cell=ReadCell(c);if(cell.x>=0&&cell.z>=0&&cell.x<width_&&cell.z<height_) known_[cell.z*width_+cell.x]=c.value("open",0);
  }
  for (const auto& c:state_.value("visible",Json::array())) {const Cell cell=ReadCell(c);if(Known(cell)) visible_[cell.z*width_+cell.x]=true;}
  if (!FeedbackOpen()&&state_.contains("question")&&state_["question"].is_object()) lastQuestion_=state_["question"];
  if (Own().value("clues",Json::array()).size()>static_cast<std::size_t>(previousClues)) celebration_=1.2f;
  std::optional<Cell> at;
  if (IsTeacher()&&state_.contains("teacher")&&state_["teacher"].is_object()) at=ReadCell(state_["teacher"]);
  else if (const auto* team=MyTeam();team&&(*team)["cell"].is_object()) at=ReadCell((*team)["cell"]);
  if (at) {
    if (!havePosition_) {cell_=*at;position_=World(cell_);havePosition_=true;}
    else if (cell_!=*at) {
      moveFrom_=position_;nextCell_=*at;moveProgress_=0;moving_=true;
      heading_=std::atan2(float(at->x-cell_.x),float(at->z-cell_.z))*RAD2DEG;cell_=*at;
    }
  }
  if (QuestionOpen()||(!IsTeacher()&&Own().value("moves",0)==0)) route_.clear();
  if (scenes_.Current()!=SceneId::Loading) {
    if (!IsTeacher()&&Own().value("finalSubmitted",false)) scenes_.Request(SceneId::Results);
    else if (Phase()!="lobby") scenes_.Request(SceneId::Exploration);
    else scenes_.Request(SceneId::Menu);
  }
}
void PartyGame::Update() {
  const float dt=smoke_?.12f:std::min(GetFrameTime(),.05f);
  time_+=dt;celebration_=std::max(0.0f,celebration_-dt);
  UpdateNetwork(dt);UpdateInput();scenes_.Update(dt);
  if (smoke_) UpdateSmoke();
}
void PartyGame::Draw() {
  BeginDrawing();ClearBackground(ui::Navy);scenes_.Draw();EndDrawing();
#ifndef __EMSCRIPTEN__
  if(smoke_){
    if(smokeStage_==0&&smokeFrames_==8)TakeScreenshot("smoke-loading.png");
    if(smokeStage_==1&&smokeFrames_==8)TakeScreenshot("smoke-menu.png");
    if(smokeStage_==4&&smokeFrames_==8)TakeScreenshot("smoke-exploration.png");
    if(smokeStage_==5&&smokeFrames_==8)TakeScreenshot("smoke-event.png");
  }
#endif
}
void PartyGame::UpdateInput() {
  std::string* value=nullptr;int max=24;
  if (focus_==1)value=&avatarName_;else if(focus_==2)value=&teamName_;else if(focus_==3){value=&joinCode_;max=6;}
  else if(focus_==4){value=&finalZ_;max=14;}else if(focus_==5){value=&finalP_;max=14;}
  if (!value) return;
  for(int c=GetCharPressed();c>0;c=GetCharPressed()) {
    if(c<32||c>255||value->size()>=static_cast<std::size_t>(max*2))continue;
    if(focus_==3){if(!std::isalnum(static_cast<unsigned char>(c)))continue;c=std::toupper(static_cast<unsigned char>(c));}
    if(focus_>=4&&!(c>='0'&&c<='9')&&c!='.'&&c!=','&&c!='-'&&c!='+')continue;
    int length=0;const char* encoded=CodepointToUTF8(c,&length);
    if(GetCodepointCount(value->c_str())<max)value->append(encoded,length);
  }
  if((IsKeyPressed(KEY_BACKSPACE)||IsKeyPressedRepeat(KEY_BACKSPACE))&&!value->empty()) {
    std::size_t i=value->size()-1;while(i>0&&(((*value)[i]&0xC0)==0x80))--i;value->resize(i);
  }
  if(IsKeyPressed(KEY_ESCAPE))focus_=0;
  if(IsKeyPressed(KEY_TAB)) focus_=focus_>=4?(focus_==4?5:4):(focus_==3?1:focus_+1);
}
void PartyGame::Input(int id,Rectangle bounds,const std::string& value,const char* placeholder) {
  if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&CheckCollisionPointRec(GetMousePosition(),bounds))focus_=id;
  DrawRectangleRounded(bounds,.15f,8,{16,29,43,255});
  DrawRectangleRoundedLinesEx(bounds,.15f,8,focus_==id?2:1,focus_==id?ui::Teal:ui::Line);
  const auto display=value.empty()?std::string(placeholder):value;
  const float size=ui::Canvas(1440,900).Scale(21);
  BeginScissorMode(int(bounds.x+10),int(bounds.y),int(bounds.width-20),int(bounds.height));
  ui::Text(display,bounds.x+14,bounds.y+(bounds.height-size)*.5f,size,value.empty()?ui::Muted:ui::Cream);
  if(focus_==id&&int(time_*2)%2==0) {
    const float x=bounds.x+14+(value.empty()?0:ui::Measure(value,size).x);
    DrawLineEx({x,bounds.y+12},{x,bounds.y+bounds.height-12},1,ui::Teal);
  }
  EndScissorMode();
}
void PartyGame::UpdateMenu(float dt) {
  UpdateCamera(dt,true);
  if(!token_.empty()) {
    if(Phase()!="lobby")ApplyState(state_);
    return;
  }
  if(!focus_) {
    if(IsKeyPressed(KEY_RIGHT))selected_=(selected_+1)%6;
    if(IsKeyPressed(KEY_LEFT))selected_=(selected_+5)%6;
    for(int i=0;i<6;++i)if(IsKeyPressed(KEY_ONE+i))selected_=i;
  }
  if(IsKeyPressed(KEY_ENTER)) {focus_=0;JoinRoom();}
#ifndef __EMSCRIPTEN__
  if(IsKeyPressed(KEY_ESCAPE)&&!focus_)quit_=true;
#endif
}

bool PartyGame::Known(Cell c) const { return c.x>=0&&c.z>=0&&c.x<width_&&c.z<height_&&known_[c.z*width_+c.x]>=0; }
bool PartyGame::Visible(Cell c) const { return Known(c)&&visible_[c.z*width_+c.x]; }
bool PartyGame::CanMove(Cell from,Cell to) const {
  if(!Known(from)||!Known(to))return false;
  const int dx=to.x-from.x,dz=to.z-from.z;
  int bit=dx==0&&dz==-1?1:dx==1&&dz==0?2:dx==0&&dz==1?4:dx==-1&&dz==0?8:0;
  return bit&&(known_[from.z*width_+from.x]&bit);
}
std::vector<Cell> PartyGame::KnownPath(Cell from,Cell to) const {
  if(!Known(from)||!Known(to))return {};
  std::vector<int> parents(width_*height_,-1);std::queue<Cell> queue;queue.push(from);parents[from.z*width_+from.x]=from.z*width_+from.x;
  while(!queue.empty()) {
    const Cell c=queue.front();queue.pop();if(c==to)break;
    for(Cell n:std::array<Cell,4>{{{c.x,c.z-1},{c.x+1,c.z},{c.x,c.z+1},{c.x-1,c.z}}})
      if(CanMove(c,n)&&parents[n.z*width_+n.x]<0){parents[n.z*width_+n.x]=c.z*width_+c.x;queue.push(n);}
  }
  int index=to.z*width_+to.x;if(parents[index]<0)return {};
  std::vector<Cell> path;
  while(index!=from.z*width_+from.x){path.push_back({index%width_,index/width_});index=parents[index];}
  path.push_back(from);std::reverse(path.begin(),path.end());return path;
}
void PartyGame::SetRoute(Cell destination) {
  const auto path=KnownPath(cell_,destination);route_.clear();
  for(std::size_t i=1;i<path.size();++i)route_.push_back(path[i]);
}
void PartyGame::BeginStep(Cell destination) {
  if(actionPending_||moving_||QuestionOpen()||!CanMove(cell_,destination))return;
  if(!IsTeacher()&&(Own().value("moves",0)<=0||Phase()!="playing"))return;
  SendAction(IsTeacher()?"teacher_move":"move",WriteCell(destination));
}
void PartyGame::RouteHint() {
  std::vector<Cell> best;
  for(const auto& object:state_.value("objects",Json::array())) {
    if(object.value("owner",-1)!=teamId_||object.value("resolved",false))continue;
    const auto path=KnownPath(cell_,ReadCell(object));
    if(path.size()>1&&(best.empty()||path.size()<best.size()))best=path;
  }
  if(best.empty())for(int z=0;z<height_;++z)for(int x=0;x<width_;++x) {
    Cell c{x,z};if(!Known(c)||c==cell_)continue;
    const std::array<Cell,4> ns{{{x,z-1},{x+1,z},{x,z+1},{x-1,z}}};bool frontier=false;
    for(int i=0;i<4;++i)if((known_[z*width_+x]&(1<<i))&&!Known(ns[i]))frontier=true;
    if(frontier){const auto path=KnownPath(cell_,c);if(path.size()>1&&(best.empty()||path.size()<best.size()))best=path;}
  }
  route_.clear();for(std::size_t i=1;i<best.size();++i)route_.push_back(best[i]);
  if(route_.empty())status_="No hay una ruta conocida pendiente. Explora los pasillos cercanos.";
}
void PartyGame::UpdateExploration(float dt) {
  UpdateCamera(dt);elapsed_+=dt;
  if(moving_) {
    moveProgress_=std::min(1.0f,moveProgress_+dt*3.5f);position_=Vector3Lerp(moveFrom_,World(nextCell_),moveProgress_);
    if(moveProgress_>=1)moving_=false;
  }
  if(IsKeyPressed(KEY_ESCAPE)) {
    if(journal_){journal_=false;focus_=0;}
    else if(!QuestionOpen())paused_=!paused_;
  }
  if(!focus_&&IsKeyPressed(KEY_TAB))overview_=!overview_;
  if(IsKeyPressed(KEY_J)&&!QuestionOpen()&&!paused_){journal_=!journal_;focus_=0;}
  if(paused_||journal_||(!IsTeacher()&&(Phase()=="paused"||Phase()=="finished")))return;
  if(QuestionOpen()) {
    route_.clear();
    if(FeedbackOpen()) {if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE))CloseQuestion();}
    else {if(IsKeyPressed(KEY_ONE))Answer(0);if(IsKeyPressed(KEY_TWO))Answer(1);if(IsKeyPressed(KEY_THREE))Answer(2);}
    return;
  }
  if(IsKeyDown(KEY_Q))cameraAngle_-=dt;if(IsKeyDown(KEY_E))cameraAngle_+=dt;
  zoom_=std::clamp(zoom_-GetMouseWheelMove()*2,14.0f,38.0f);
  if(IsKeyPressed(KEY_H))RouteHint();
  if(actionPending_||moving_)return;
  const auto mouse=GetMousePosition();
  if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&CheckCollisionPointRec(mouse,Rect(16,199,1050,595))) {
    const Ray ray=GetScreenToWorldRay(mouse,camera_);
    if(ray.direction.y<-.001f){const auto hit=Vector3Add(ray.position,Vector3Scale(ray.direction,-ray.position.y/ray.direction.y));
      const Cell c{int(std::floor(hit.x/Tile+.5f)),int(std::floor(hit.z/Tile+.5f))};if(Known(c))SetRoute(c);}
  }
  Cell to=cell_;bool manual=false;
  if(IsKeyDown(KEY_W)||IsKeyDown(KEY_UP)){--to.z;manual=true;}
  else if(IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN)){++to.z;manual=true;}
  else if(IsKeyDown(KEY_A)||IsKeyDown(KEY_LEFT)){--to.x;manual=true;}
  else if(IsKeyDown(KEY_D)||IsKeyDown(KEY_RIGHT)){++to.x;manual=true;}
  if(manual){route_.clear();BeginStep(to);}
  else if(!route_.empty()){const auto next=route_.front();route_.pop_front();BeginStep(next);}
}
void PartyGame::UpdateResults(float dt) { UpdateCamera(dt,true); }
void PartyGame::UpdateCamera(float dt,bool menu) {
  Vector3 target,desired;
  if(menu){target={-.8f,1.3f,0};desired={10,8,15.5f};}
  else {
    const float distance=overview_?67:zoom_,angle=.60f+cameraAngle_;
    target=overview_?Vector3{18,0,18}:Vector3{std::clamp(position_.x+2,4.0f,32.0f),0,std::clamp(position_.z+3,4.0f,32.0f)};
    desired=Vector3Add(target,{-std::sin(angle)*distance,distance*.98f,-std::cos(angle)*distance});
  }
  const float smoothing=std::min(1.0f,dt*7);camera_.target=Vector3Lerp(camera_.target,target,smoothing);camera_.position=Vector3Lerp(camera_.position,desired,smoothing);
}
void PartyGame::Answer(int choice) {
  if(FeedbackOpen()||actionPending_||!state_.contains("question")||!state_["question"].is_object())return;
  lastQuestion_=state_["question"];
  SendAction(lastQuestion_.value("kind","")=="hard"?"hard_answer":"easy_answer",{{"questionId",lastQuestion_.value("id",-1)},{"choice",choice}});
}
void PartyGame::CloseQuestion() {
  if(FeedbackOpen())dismissedFeedback_=state_["feedback"].value("questionId",-1);
  if(state_.contains("question")&&state_["question"].is_object())lastQuestion_=state_["question"];
  ui::ResetInput();
}
void PartyGame::SubmitHypothesis() {
  double z=0,p=0;
  if(finalAlternative_<0||finalDecision_<0||!ReadNumber(finalZ_,z)||!ReadNumber(finalP_,p)||p<0||p>1) {
    status_="Completa H1, z, p entre 0 y 1, y la decisión.";return;
  }
  SendAction("final",{{"alternative",finalAlternative_},{"z",z},{"p",p},{"reject",finalDecision_==1}});
}

void PartyGame::DrawMenu() {
  if(!token_.empty()&&state_.contains("teams")){DrawLobby();return;}
  DrawWorld(true);DrawRectangleRec(Rect(0,93,655,694),Alpha(ui::Navy,.98f));DrawChrome("01 / ENTRA CON TU EQUIPO");
  Text("CADA EQUIPO, SU PROPIA HIPÓTESIS",54,120,16,ui::Teal);
  Text("El laberinto de las ideas",50,151,42,ui::Gold);
  if(Button(54,223,244,43,"Soy estudiante",!teacherChoice_)){teacherChoice_=false;focus_=1;}
  if(Button(314,223,272,43,"Soy la profesora",teacherChoice_)){teacherChoice_=true;focus_=1;}
  Text("NOMBRE DE TU AVATAR",54,295,16,ui::Muted);Input(1,Rect(54,321,532,50),avatarName_,"El nombre que verán los demás");
  if(!teacherChoice_) {
    Text("EQUIPO",54,394,16,ui::Muted);Input(2,Rect(54,420,532,50),teamName_,"Nombre del equipo");
    Text("CÓDIGO DE LA SALA",54,493,16,ui::Muted);Input(3,Rect(54,519,532,50),joinCode_,"Seis caracteres");
  } else {
    DrawTextWrapped("Crea la sala y comparte su enlace y código. Podrás iniciar y pausar la partida, ver los equipos y caminar con tu avatar.",Rect(54,404,515,145),21,ui::Muted);
  }
  if(Button(54,610,532,57,actionPending_?"Conectando…":(teacherChoice_?"Crear sala de clase":"Entrar al equipo"),true,!actionPending_)) {focus_=0;JoinRoom();}
  if(!status_.empty())DrawTextWrapped(status_,Rect(54,689,535,66),16,ui::Error);
  else Text("Un dispositivo por equipo · hasta seis equipos",54,704,16,ui::Muted);
  Text(teacherChoice_?(avatarName_.empty()?"TU AVATAR DE PROFESORA":avatarName_):"ELIGE TU ESTILO",927,140,20,ui::Gold);
  DrawRectangleRec(Rect(0,787,1440,113),ui::Navy);
  for(int i=0;i<6;++i)if(Button(34+i*232,810,213,57,"Avatar "+std::to_string(i+1),!teacherChoice_&&selected_==i,!teacherChoice_))selected_=i;
}
void PartyGame::DrawLobby() {
  DrawWorld(true);DrawRectangleRec(Rect(0,93,755,807),Alpha(ui::Navy,.97f));DrawChrome("SALA / ESPERANDO A LOS EQUIPOS");
  Text("CÓDIGO PARA COMPARTIR",54,133,18,ui::Teal);Text(code_,50,166,72,ui::Gold);
  Text(IsTeacher()?"Tu aula está abierta.":"Tu equipo ya está dentro.",54,267,32);
  DrawTextWrapped(IsTeacher()?"Cada equipo entra desde este mismo enlace en su dispositivo, escribe este código y elige el nombre de su avatar.":"La profesora iniciará cuando estén listos. Tu equipo compartirá movimientos, pistas y puntuación.",Rect(54,321,640,95),20,ui::Muted);
  int index=0;
  for(const auto& team:state_.value("teams",Json::array())) {
    ui::Panel(Rect(54,439+index*47,640,40));
    Text(team.value("name","")+"  /  "+team.value("avatarName",""),70,449+index*47,18,GlyphColors[index%6]);++index;
  }
  if(index==0)Text("Esperando al primer equipo…",54,455,23,ui::Muted);
  if(IsTeacher()&&Button(54,761,640,58,"Iniciar partida",true,index>0&&!actionPending_)){focus_=0;SendAction("start");}
  if(!IsTeacher())Text("Esperando el inicio de la profesora…",54,778,21,ui::Teal);
  if(!status_.empty())DrawTextWrapped(status_,Rect(54,842,640,45),16,ui::Error);
}
void PartyGame::DrawWorld(bool lobby,bool results) {
  struct WorldLabel { std::string text; Vector3 at; Color color; };
  std::vector<WorldLabel> labels;
  const auto avatars=lobby?std::vector<AvatarVisual>{}:VisibleAvatars();
  if(shaderReady_)SetShaderValue(lighting_,lighting_.locs[SHADER_LOC_VECTOR_VIEW],&camera_.position,SHADER_UNIFORM_VEC3);
  BeginMode3D(camera_);
  if(lobby) {
    DrawCube({1,-.35f,0},26,.5f,18,{25,40,51,255});
    for(int x=-3;x<=4;++x)for(int z=-2;z<=2;++z)DrawProp("floor",{x*Tile,-.08f,z*Tile});
    for(int i=0;i<5;++i){DrawProp("pillar",{-7.5f+i*4.5f,0,-6});if(i<4)DrawProp("wall",{-5.3f+i*4.5f,0,-6},{1.5f,.72f,1});}
    if(results) {
      const auto teams=RankedTeams();
      for(int i=0;i<std::min(3,static_cast<int>(teams.size()));++i) {
        const auto& team=teams[i];
        const float x=5+(i==1?-3.0f:i==2?3.0f:0),height=i==0?1.4f:i==1?.9f:.65f;
        const Vector3 at{x,height*.65f,0};
        DrawProp("podium",{x,0,0},{1,height,1});
        DrawCharacter(Characters[std::clamp(team.value("avatarIndex",0),0,5)],at,20,
          Phase()=="finished"?"Celebrate":"Idle",time_+i,1.2f);
        labels.push_back({team.value("name","")+"\n"+team.value("avatarName","")+" · "+
          std::to_string(team.value("score",0))+" pts",{at.x,at.y+2.6f,at.z},
          GlyphColors[std::clamp(team.value("id",0),0,5)]});
      }
      DrawCharacter("professor",{5.6f,0,-4},20,Phase()=="finished"?"Celebrate":"Idle",time_,1.25f);
    }else {
      const bool professor=teacherChoice_||IsTeacher();
      DrawProp("podium",{3.5f,0,0},{1.6f,.8f,1.6f});
      DrawCharacter(professor?"professor":Characters[selected_],{3.5f,.52f,0},28,"Idle",time_,1.9f);
      DrawProp("podium",{6.5f,0,-3},{1,.7f,1});DrawCharacter(professor?Characters[0]:"professor",{6.5f,.455f,-3},15,"Idle",time_+1,1.3f);
      DrawCharacter(Characters[(selected_+1)%6],{.5f,0,-3.2f},25,"Idle",time_);
    }
    for(int i=0;i<3;++i)DrawGlyph({i*2.3f+1,2.3f,-4.5f},i,.8f);
  }else {
    DrawMaze(avatars);
    for(const auto& avatar:avatars)
      labels.push_back({avatar.name,{avatar.at.x,avatar.at.y+2.25f,avatar.at.z},avatar.color});
  }
  EndMode3D();
  const ui::Canvas canvas(1440,900);
  const float padding=canvas.Scale(7),gap=canvas.Scale(4);
  const Rectangle viewport=Rect(results?800:12,results?175:105,results?625:1416,results?680:695);
  std::vector<Rectangle> occupied;
  // Place the lowest labels first, then lift overlaps and retain a small leader line.
  std::stable_sort(labels.begin(),labels.end(),[this](const auto& a,const auto& b){
    return GetWorldToScreen(a.at,camera_).y>GetWorldToScreen(b.at,camera_).y;
  });
  for(const auto& label:labels) {
    if(label.text.empty()||Vector3DotProduct(Vector3Subtract(label.at,camera_.position),
       Vector3Subtract(camera_.target,camera_.position))<=0)continue;
    const auto anchor=GetWorldToScreen(label.at,camera_);
    if(anchor.x<0||anchor.x>GetScreenWidth()||anchor.y<viewport.y||anchor.y>viewport.y+viewport.height)continue;
    float size=canvas.Scale(16);
    auto measured=ui::Measure(label.text,size);
    const float maxWidth=canvas.Scale(results?205:260);
    if(measured.x>maxWidth){size*=maxWidth/measured.x;measured=ui::Measure(label.text,size);}
    Rectangle box{anchor.x-measured.x*.5f-padding,anchor.y-padding,measured.x+2*padding,measured.y+2*padding};
    box.x=std::clamp(box.x,viewport.x,std::max(viewport.x,viewport.x+viewport.width-box.width));
    for(std::size_t attempt=0;attempt<=occupied.size();++attempt) {
      bool overlap=false;
      for(const auto& other:occupied)if(CheckCollisionRecs(box,other)) {
        box.y=other.y-box.height-gap;overlap=true;break;
      }
      if(!overlap)break;
    }
    if(box.y<viewport.y)box.y=viewport.y;
    occupied.push_back(box);
    const Vector2 connector{std::clamp(anchor.x,box.x+padding,box.x+box.width-padding),box.y+box.height};
    if(Vector2Distance(connector,anchor)>canvas.Scale(16))
      DrawLineEx(connector,anchor,canvas.Scale(1),Alpha(label.color,.65f));
    DrawRectangleRounded(box,.2f,5,Alpha(ui::Navy,.92f));
    ui::Text(label.text,box.x+padding,box.y+padding,size,label.color);
  }
  if(results)Text(Phase()=="finished"?"CLASIFICACIÓN FINAL":"CLASIFICACIÓN ACTUAL",826,137,18,ui::Gold);
}
void PartyGame::DrawMaze(const std::vector<AvatarVisual>& avatars) {
  const float extent=width_*Tile;
  DrawCube({(extent-Tile)*.5f,-.4f,(extent-Tile)*.5f},extent+1,.5f,extent+1,{9,17,27,255});
  for(int z=0;z<height_;++z)for(int x=0;x<width_;++x) {
    const Cell c{x,z};if(!Known(c))continue;
    const auto p=World(c);const bool visible=Visible(c)||IsTeacher();const auto tint=visible?WHITE:Color{75,88,104,255};
    DrawProp("floor",p,{1,1,1},0,tint);const int mask=known_[z*width_+x];
    auto wall=[&](Vector3 at,float rotation){DrawProp("wall",at,{1,Vector3Distance(at,position_)<4.5f?.55f:1,1},rotation,tint);};
    if(!(mask&2))wall({p.x+1.5f,0,p.z},90);if(!(mask&4))wall({p.x,0,p.z+1.5f},0);
    if(z==0||!Known({x,z-1})){if(!(mask&1))wall({p.x,0,p.z-1.5f},0);}
    if(x==0||!Known({x-1,z})){if(!(mask&8))wall({p.x-1.5f,0,p.z},90);}
  }
  for(const auto& object:state_.value("objects",Json::array())) {
    if(object.value("resolved",false))continue;const Cell c=ReadCell(object);if(!Visible(c)&&!IsTeacher())continue;
    const int owner=object.value("owner",0);auto p=World(c);
    if(object.value("kind","")=="clue") {DrawProp("podium",p,{.45f,.27f,.45f});DrawGlyph({p.x,.15f,p.z},owner,.65f);}
    else DrawGlyph(p,owner);
  }
  for(const auto& avatar:avatars) {
    DrawCharacter(avatar.model,avatar.at,avatar.own?heading_:180,
      avatar.own&&moving_?"Walk":avatar.own&&celebration_>0?"Celebrate":"Idle",time_);
    if(avatar.own)DrawCircle3D({avatar.at.x,.04f,avatar.at.z},.48f,{1,0,0},90,avatar.color);
  }
  for(const Cell c:route_)if(Known(c)){const auto p=World(c);DrawSphereEx({p.x,.08f,p.z},.065f,3,5,ui::Teal);}
}

void PartyGame::DrawMinimap(Rectangle rect) {
  ui::Panel(rect,{12,24,36,255});const float pad=rect.width*.06f,unit=(rect.width-2*pad)/width_;
  auto point=[&](Cell c){return Vector2{rect.x+pad+(c.x+.5f)*unit,rect.y+pad+(c.z+.5f)*unit};};
  for(int z=0;z<height_;++z)for(int x=0;x<width_;++x) {
    Cell c{x,z};if(!Known(c))continue;const float l=rect.x+pad+x*unit,t=rect.y+pad+z*unit;const int mask=known_[z*width_+x];
    DrawRectangleRec({l,t,unit,unit},Visible(c)||IsTeacher()?Color{39,64,73,255}:Color{23,37,50,255});
    const Color wall=Visible(c)||IsTeacher()?Color{115,147,157,255}:ui::Line;
    if(!(mask&1))DrawLineEx({l,t},{l+unit,t},1,wall);if(!(mask&2))DrawLineEx({l+unit,t},{l+unit,t+unit},1,wall);
    if(!(mask&4))DrawLineEx({l,t+unit},{l+unit,t+unit},1,wall);if(!(mask&8))DrawLineEx({l,t},{l,t+unit},1,wall);
  }
  for(const auto& object:state_.value("objects",Json::array()))if(!object.value("resolved",false)) {
    const auto p=point(ReadCell(object));const auto color=GlyphColors[object.value("owner",0)%6];
    if(object.value("kind","")=="clue")DrawRectangleV({p.x-unit*.2f,p.y-unit*.2f},{unit*.4f,unit*.4f},color);
    else DrawCircleV(p,unit*.22f,color);
  }
  for(const auto& team:state_.value("teams",Json::array()))if(team["cell"].is_object())DrawCircleV(point(ReadCell(team["cell"])),unit*.25f,GlyphColors[team.value("id",0)%6]);
  if(state_.contains("teacher")&&state_["teacher"].is_object())DrawCircleV(point(ReadCell(state_["teacher"])),unit*.25f,ui::Gold);
  DrawCircleLinesV(point(cell_),unit*.43f,ui::Cream);
}
void PartyGame::DrawExploration() {
  DrawWorld();DrawChrome(IsTeacher()?"PROFESORA / VISTA DE LA CLASE":"02 / EXPLORA Y REÚNE EVIDENCIA");
  ui::Panel(Rect(32,112,480,77));
  const auto* team=MyTeam();
  const std::string name=IsTeacher()?state_.value("teacherName","Profesora"):(team?team->value("avatarName",""):avatarName_);
  Text(name,52,128,24,IsTeacher()?ui::Gold:ui::Cream);
  Text(IsTeacher()?"ANFITRIONA · MOVIMIENTO LIBRE":(team?team->value("name",""):teamName_)+"  /  "+std::to_string(Own().value("score",0))+" puntos",52,163,15,ui::Muted);
  ui::Panel(Rect(1090,112,318,321));
  if(IsTeacher()) {
    Text("SALA "+code_,1110,135,22,ui::Gold);Text("Vista de profesora",1110,178,23);
    DrawTextWrapped("Tu avatar puede recorrer los pasillos. Cada equipo descubre su mapa y recibe pistas diferentes.",Rect(1110,220,272,105),18,ui::Muted);
    if(Button(1110,350,276,48,Phase()=="paused"?"Reanudar clase":"Pausar clase",true,!actionPending_&&Phase()!="finished"))SendAction(Phase()=="paused"?"resume":"pause");
  }else {
    Text("MOVIMIENTOS DISPONIBLES",1110,136,15,ui::Gold);Text(std::to_string(Own().value("moves",0))+" pasos",1110,165,40);
    Text("Glifos: "+std::to_string(Own().value("hardAnswered",0))+" / 10",1110,230,20);
    Text("Pistas propias: "+std::to_string(Own().value("clues",Json::array()).size())+" / 4",1110,266,20,ui::Teal);
    ui::ProgressBar(Rect(1110,309,276,7),Own().value("hardAnswered",0)/10.0f);
    DrawTextWrapped("Fácil: 5 pasos si aciertas, 2 si fallas. Glifo difícil: 100 puntos.",Rect(1110,339,272,70),17,ui::Muted);
  }
  Text(IsTeacher()?"MAPA COMPLETO":"TU MAPA DESCUBIERTO",1110,452,16,ui::Gold);DrawMinimap(Rect(1090,485,318,305));
  DrawRectangleRec(Rect(20,818,1400,67),ui::Navy);
  Text("WASD / Flechas  Mover",40,842,16);Text("Clic  Ruta conocida",292,842,16,ui::Muted);
  Text(IsTeacher()?"Tab  Vista aérea":"H  Explorar  ·  Tab  Vista aérea",520,842,16,ui::Muted);
  const bool hud=!QuestionOpen()&&!journal_&&!paused_;
  if(Button(920,831,244,42,IsTeacher()?"Clasificación  [J]":"Mi hipótesis  [J]",false,hud))journal_=true;
  if(Button(1180,831,203,42,"Menú  [Esc]",false,hud))paused_=true;
  if(!status_.empty()) {
    ui::Panel(Rect(32,735,1008,55),Alpha(ui::Navy,.95f));DrawTextWrapped(status_,Rect(48,750,973,38),16,ui::Error);
  }
  if(journal_){if(IsTeacher()){
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Alpha(ui::Navy,.84f));ui::Panel(Rect(260,160,920,585));
    Text("La clase en tiempo real",300,199,35,ui::Gold);DrawLeaderboard(Rect(300,274,840,360));
    if(Button(300,660,840,51,"Volver al laberinto",true))journal_=false;
  }else DrawHypothesis();return;}
  if(paused_||(!IsTeacher()&&(Phase()=="paused"||Phase()=="finished"))){DrawPause();return;}
  if(QuestionOpen())DrawQuestion();
}
void PartyGame::DrawQuestion() {
  const auto& question=ActiveQuestion();const bool feedback=FeedbackOpen();
  const bool hard=question.value("kind","")=="hard";
  DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Alpha(ui::Navy,.84f));ui::Panel(Rect(272,120,896,660),{21,38,53,255});
  Text(hard?"GLIFO DIFÍCIL  /  +100 PUNTOS":"PREGUNTA FÁCIL  /  GANA MOVIMIENTOS",312,155,17,hard?ui::Gold:ui::Teal);
  Text(question.value("title",feedback?"Respuesta registrada":"Pregunta"),309,192,35);
  DrawTextWrapped(question.value("prompt",""),Rect(312,251,800,95),23);
  if(question.contains("options"))for(int i=0;i<3;++i)
    if(Button(312,368+i*62,816,51,std::to_string(i+1)+". "+question["options"][i].get<std::string>(),false,!feedback&&!actionPending_))Answer(i);
  if(feedback) {
    const auto& f=state_["feedback"];const bool correct=f.value("correct",false);
    std::string result=correct?"¡Correcto!":"Respuesta incorrecta.";
    const int moves=f.value("movesGranted",0),points=f.value("scoreAwarded",0);
    if(moves>0)result+=" Tienes "+std::to_string(moves)+" movimientos.";else result+=" +"+std::to_string(points)+" puntos.";
    Text(result,312,574,22,correct?ui::Teal:ui::Gold);
    DrawTextWrapped(f.value("explanation",""),Rect(312,614,800,82),17,ui::Muted);
    if(Button(312,713,816,45,"Continuar  [Enter]",true,!actionPending_))CloseQuestion();
  }else {
    Text(hard?"Una respuesta por glifo. Las pistas solo las recoge tu equipo.":"Acertar permite avanzar 5 espacios; fallar permite avanzar 2.",312,596,17,ui::Muted);
    Text(actionPending_?"Comprobando respuesta…":"Elige con el mouse o las teclas 1, 2 y 3.",312,651,18,ui::Teal);
  }
}
void PartyGame::DrawHypothesis() {
  DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Alpha(ui::Navy,.88f));ui::Panel(Rect(120,105,1200,700),{21,38,53,255});
  Text("TU EXPEDIENTE  /  HASTA 400 PUNTOS",158,138,17,ui::Teal);
  const auto challenge=Own().value("hypothesis",Json::object());
  Text(challenge.value("title","Hipótesis del equipo"),155,173,34,ui::Gold);
  const auto clues=Own().value("clues",Json::array());
  const Rectangle clueViewport=Rect(158,239,535,452);
  std::array<float,4> heights{};float totalHeight=0;
  const float scale=ui::Canvas(1440,900).GetScale();
  for(int i=0;i<4;++i) {
    int lines=1;
    if(i<static_cast<int>(clues.size())) {
      std::istringstream words(clues[i].value("text",""));std::string word,line;
      while(words>>word){const std::string candidate=line.empty()?word:line+" "+word;
        if(!line.empty()&&ui::Measure(candidate,16*scale).x>502*scale){++lines;line=word;}else line=candidate;}
    }
    heights[i]=std::max(102.0f,57.0f+lines*24.0f);totalHeight+=heights[i]+13;
  }
  if(CheckCollisionPointRec(GetMousePosition(),clueViewport))clueScroll_-=GetMouseWheelMove()*42;
  if(!focus_&&IsKeyPressed(KEY_PAGE_DOWN))clueScroll_+=180;
  if(!focus_&&IsKeyPressed(KEY_PAGE_UP))clueScroll_-=180;
  clueScroll_=std::clamp(clueScroll_,0.0f,std::max(0.0f,totalHeight-452));
  BeginScissorMode(int(clueViewport.x),int(clueViewport.y),int(clueViewport.width),int(clueViewport.height));
  float clueY=239-clueScroll_;
  for(int i=0;i<4;++i) {
    const float y=clueY;ui::Panel(Rect(158,y,535,heights[i]));
    if(i<static_cast<int>(clues.size())) {
      Text(clues[i].value("title","Pista"),174,y+12,18,ui::Teal);
      DrawTextWrapped(clues[i].value("text",""),Rect(174,y+41,502,heights[i]-49),16,ui::Cream);
    }else {Text("Pista "+std::to_string(i+1)+" por encontrar",174,y+21,20,ui::Muted);Text("Esta evidencia es exclusiva de tu equipo.",174,y+61,15,ui::Muted);}
    clueY+=heights[i]+13;
  }
  EndScissorMode();
  Text("Desplaza la rueda sobre las pistas para leerlas completas.",158,703,14,ui::Muted);
  const bool ready=Own().value("canSubmitFinal",false);
  if(ready) {
    Text("1. ELIGE H1  /  100 PUNTOS",730,239,16,ui::Gold);
    const auto options=challenge.value("alternatives",Json::array({"mu < mu0","mu != mu0","mu > mu0"}));
    for(int i=0;i<3;++i)if(Button(730,269+i*43,550,35,options[i].get<std::string>(),finalAlternative_==i,!actionPending_)){finalAlternative_=i;focus_=0;}
    Text("2. ESTADÍSTICO z  /  100",730,412,16,ui::Gold);Input(4,Rect(730,439,250,45),finalZ_,"Ej. -1.96");
    Text("3. VALOR p  /  100",1010,412,16,ui::Gold);Input(5,Rect(1010,439,270,45),finalP_,"Ej. 0.050");
    Text("z: dos decimales · p: tres o más decimales",730,496,15,ui::Muted);
    Text("4. DECISIÓN  /  100 PUNTOS",730,537,16,ui::Gold);
    if(Button(730,566,267,47,"No rechazar H0",finalDecision_==0,!actionPending_)){finalDecision_=0;focus_=0;}
    if(Button(1013,566,267,47,"Rechazar H0",finalDecision_==1,!actionPending_)){finalDecision_=1;focus_=0;}
    Text("No rechazar H0 no significa demostrarla.",730,629,16,ui::Muted);
    if(Button(730,677,550,54,actionPending_?"Calificando…":"Entregar resolución · un intento",true,!actionPending_))SubmitHypothesis();
  }else {
    Text("Reúne y calcula en equipo",730,247,28);
    DrawTextWrapped("Encuentra las cuatro pistas y responde los diez glifos para desbloquear la entrega. Cada dato cambia la prueba: revisa H1, el tamaño de muestra, sigma y alfa.",Rect(730,303,520,150),21,ui::Muted);
    Text(std::to_string(clues.size())+" / 4 pistas",730,503,30,ui::Teal);
    Text(std::to_string(Own().value("hardAnswered",0))+" / 10 glifos respondidos",730,553,27,ui::Gold);
    DrawTextWrapped("Las preguntas fáciles solo dan pasos. Las difíciles dan puntos; el reto final vale otros 400.",Rect(730,618,520,88),18,ui::Muted);
  }
  if(Button(158,730,535,47,"Volver a explorar",false,!actionPending_)){journal_=false;focus_=0;}
  if(!status_.empty())DrawTextWrapped(status_,Rect(730,748,550,46),15,ui::Error);
}
void PartyGame::DrawPause() {
  DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Alpha(ui::Navy,.82f));ui::Panel(Rect(420,235,600,445));
  const bool global=Phase()=="paused",finished=Phase()=="finished";
  Text(finished?"La clase terminó":global?"La profesora pausó la clase":"Un momento para pensar",460,282,31,ui::Gold);
  Text("Sala "+code_,460,340,24,ui::Teal);
  DrawTextWrapped(finished?"Todos los equipos entregaron su hipótesis. Revisa la clasificación y compartan sus conclusiones.":"Tu progreso y tus pistas se conservan en el servidor mientras la sala siga abierta.",Rect(460,391,520,90),20,ui::Muted);
  if(global&&IsTeacher()) {if(Button(460,522,520,53,"Reanudar la clase",true,!actionPending_))SendAction("resume");}
  else if(!global&&!finished) {if(Button(460,522,520,53,"Continuar explorando",true))paused_=false;}
  else if(global)Text("Espera a que la profesora reanude la partida.",460,540,18,ui::Muted);
  if(Button(460,601,520,44,IsTeacher()?"Ver clasificación":"Ver mis pistas")){paused_=false;journal_=true;}
}
void PartyGame::DrawLeaderboard(Rectangle area) {
  const auto teams=RankedTeams();
  const float scale=ui::Canvas(1440,900).GetScale();int i=0;
  const float row=std::min(55.0f*scale,area.height/std::max(1,int(teams.size())));
  for(const auto& team:teams) {
    const float y=area.y+i*row;
    ui::Panel({area.x,y,area.width,row-5*scale});
    ui::Text(std::to_string(i+1)+". "+team.value("name","")+" / "+team.value("avatarName",""),area.x+14*scale,y+(row-18*scale)*.5f-2*scale,17*scale,ui::Cream);
    ui::Text(std::to_string(team.value("score",0))+" pts",area.x+area.width-130*scale,y+(row-18*scale)*.5f-2*scale,17*scale,ui::Gold);++i;
  }
}
void PartyGame::DrawResults() {
  DrawWorld(true,true);DrawRectangleRec(Rect(0,93,780,807),Alpha(ui::Navy,.98f));DrawChrome("03 / HIPÓTESIS ENTREGADA");
  const auto* team=MyTeam();if(team)selected_=std::clamp(team->value("avatarIndex",0),0,5);
  Text("EXPEDICIÓN DE EQUIPO COMPLETADA",54,130,17,ui::Teal);Text("La evidencia decide.",50,170,52,ui::Gold);
  Text(team?team->value("name",""):teamName_,54,248,28);
  Text(std::to_string(Own().value("score",0))+" / 1400 puntos",54,294,40);
  const auto assessment=Own().value("assessment",Json::object());
  const auto criteria=assessment.value("criteria",Json::array({false,false,false,false}));
  const std::array<const char*,4> labels{"Formulación de H1","Estadístico z","Valor p","Decisión sobre H0"};
  for(int i=0;i<4;++i)Text(std::string(labels[i])+"  ·  "+(criteria[i].get<bool>()?"100 / 100":"0 / 100"),54,369+i*38,21,criteria[i].get<bool>()?ui::Teal:ui::Muted);
  Text(TextFormat("Referencia: z = %.3f   ·   p = %.4f",assessment.value("expectedZ",0.0),assessment.value("expectedP",0.0)),54,546,20,ui::Gold);
  Text(assessment.value("expectedReject",false)?"Decisión: rechazar H0.":"Decisión: no rechazar H0.",54,583,21);
  Text("CLASIFICACIÓN ACTUAL DE LA CLASE",54,642,16,ui::Muted);DrawLeaderboard(Rect(54,677,660,205));
}

void PartyGame::UpdateSmoke() {
  ++smokeFrames_;
  if(smokeFrames_>3000){TraceLog(LOG_ERROR,"SMOKE: network timeout");quit_=true;exitCode_=1;return;}
  if(smokeStage_==0&&resources_.IsLoadingComplete()&&smokeFrames_>25){
    PrepareMaterials();if(!ValidateAssets()){quit_=true;exitCode_=1;return;}
    scenes_.Request(SceneId::Menu);smokeStage_=1;smokeFrames_=0;
  }else if(smokeStage_==1&&smokeFrames_>12&&!actionPending_){
    teacherChoice_=true;avatarName_="Profesora Smoke";JoinRoom();smokeStage_=2;smokeFrames_=0;
  }else if(smokeStage_==2&&!token_.empty()&&IsTeacher()&&!actionPending_){
    teacherSmokeToken_=token_;
    actionPending_=true;Enqueue({"POST","/api/join",{{"code",code_},{"teamName","Equipo Smoke"},{"avatarName","Ada Smoke"},{"avatarIndex",2}},true});
    smokeStage_=3;smokeFrames_=0;
  }else if(smokeStage_==3&&!IsTeacher()&&!token_.empty()&&!actionPending_){
    actionPending_=true;Enqueue({"POST","/api/action",{{"code",code_},{"token",teacherSmokeToken_},{"action","start"},{"requestId","smoke-start-"+std::to_string(requestEpoch_)}},true});
    smokeStage_=4;smokeFrames_=0;
  }else if(smokeStage_==4&&Phase()=="playing"&&QuestionOpen()&&!actionPending_&&smokeFrames_>12){
    smokeStage_=5;smokeFrames_=0;
  }else if(smokeStage_==5&&smokeFrames_==12){Answer(0);}
  else if(smokeStage_==5&&smokeFrames_>20&&FeedbackOpen()&&!actionPending_){
    CloseQuestion();const int moves=Own().value("moves",0);
    const bool pass=state_.value("cells",Json::array()).size()<169&&(moves==2||moves==5)&&!IsTeacher();
    TraceLog(pass?LOG_INFO:LOG_ERROR,"SMOKE: %s | online room, named team, private fog, 2/5 movement question",pass?"PASS":"FAIL");
    if(pass)TakeScreenshot("smoke-fog.png");exitCode_=pass?0:1;quit_=true;
  }
}
} // namespace hp
