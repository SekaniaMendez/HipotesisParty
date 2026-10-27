#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "Maze.h"
#include "PartyUI.h"
#include "ResourceManager.h"
#include "SceneManager.h"
#include "SessionClient.h"
namespace hp {
class PartyGame {
 public:
  explicit PartyGame(std::uint32_t seed, bool smokeTest = false);
  ~PartyGame();
  void Update();
  void Draw();
  bool ShouldQuit() const { return quit_; }
  int ExitCode() const { return exitCode_; }
 private:
  using Json=nlohmann::json;
  struct Request { std::string method,path; Json body; bool action=false; };
  struct AvatarVisual {
    int id;
    std::string model,name;
    Cell cell;
    Vector3 at;
    Color color;
    bool own;
  };
  ResourceManager resources_;
  SceneManager scenes_;
  SessionClient client_;
  Json state_=Json::object(), lastQuestion_=Json::object();
  std::deque<Request> requests_;
  std::optional<Request> inFlight_;
  std::string token_,code_,role_,avatarName_,teamName_,joinCode_,status_;
  std::string finalZ_,finalP_,assetRoot_;
  std::string teacherSmokeToken_;
  std::uint32_t seed_;
  std::uint64_t sequence_=0,requestEpoch_=0;
  int teamId_=-1,selected_=0,focus_=0,finalAlternative_=-1,finalDecision_=-1;
  int dismissedFeedback_=-1,width_=13,height_=13;
  std::vector<int> known_;
  std::vector<bool> visible_;
  Shader lighting_{};
  Camera3D camera_{};
  std::deque<Cell> route_;
  Cell cell_{},nextCell_{};
  Vector3 position_{},moveFrom_{};
  float moveProgress_=0,heading_=180,time_=0,elapsed_=0,pollTimer_=0;
  float cameraAngle_=0,zoom_=22,celebration_=0,clueScroll_=0;
  int smokeFrames_=0,smokeStage_=0,exitCode_=0;
  bool moving_=false,overview_=false,paused_=false,teacherChoice_=false;
  bool smoke_=false,quit_=false,shaderReady_=false,actionPending_=false;
  bool journal_=false,networkError_=false,havePosition_=false;
  void QueueAssets(); void PrepareMaterials(); void RegisterScenes();
  void UpdateLoading(float dt); void UpdateMenu(float dt);
  void UpdateExploration(float dt); void UpdateResults(float dt);
  void UpdateCamera(float dt,bool menu=false); void UpdateSmoke();
  void UpdateNetwork(float dt); void ApplyState(const Json& state);
  void JoinRoom(); void SendAction(const std::string& action,Json extra=Json::object());
  void Enqueue(Request request); void ResetAccess();
  bool IsTeacher() const { return role_=="teacher"; }
  std::string Phase() const; const Json& Own() const;
  const Json& ActiveQuestion() const; bool FeedbackOpen() const; bool QuestionOpen() const;
  const Json* MyTeam() const; std::string AvatarId() const;
  bool Known(Cell cell) const; bool Visible(Cell cell) const; bool CanMove(Cell from,Cell to) const;
  std::vector<Cell> KnownPath(Cell from,Cell to) const;
  void SetRoute(Cell destination); void BeginStep(Cell destination); void RouteHint();
  void Answer(int choice); void CloseQuestion(); void SubmitHypothesis();
  void UpdateInput(); void Input(int id,Rectangle rect,const std::string& value,const char* placeholder);
  void DrawLoading(); void DrawMenu(); void DrawLobby();
  void DrawExploration(); void DrawResults(); void DrawQuestion(); void DrawHypothesis();
  void DrawPause(); void DrawLeaderboard(Rectangle area);
  std::vector<AvatarVisual> VisibleAvatars() const;
  Json RankedTeams() const;
  void DrawWorld(bool lobby=false,bool results=false); void DrawMaze(const std::vector<AvatarVisual>& avatars);
  void DrawGlyph(Vector3 position,int id,float scale=1.0f);
  void DrawCharacter(const std::string& id,Vector3 at,float heading,const char* clip,float clock,float scale=1.0f);
  void DrawProp(const char* id,Vector3 at,Vector3 scale={1,1,1},float rotation=0,Color tint=WHITE);
  void DrawMinimap(Rectangle rect); void DrawChrome(const char* section);
  void DrawTextWrapped(const std::string& text,Rectangle area,float size,Color color=ui::Cream);
  bool ValidateAssets(); Vector3 World(Cell cell) const;
  Rectangle Rect(float x,float y,float w,float h) const;
  void Text(const std::string& text,float x,float y,float size,Color color=ui::Cream) const;
  bool Button(float x,float y,float w,float h,const std::string& label,bool accent=false,bool enabled=true);
};
}
