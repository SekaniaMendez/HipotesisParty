#include "SessionClient.h"
#include <cstdlib>
#include <random>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(void, SendBrowserRequest, (int id, const char* method, const char* path, const char* body), {
  Module.hpReplies = Module.hpReplies || {};
  const verb = UTF8ToString(method), url = UTF8ToString(path), payload = UTF8ToString(body);
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 12000);
  const options = {method:verb, signal:controller.signal, credentials:'same-origin', cache:'no-store'};
  if (payload) { options.headers={'Content-Type':'application/json'}; options.body=payload; }
  fetch(url,options).then(async response => {
    const data = await response.text();
    try { JSON.parse(data); Module.hpReplies[id]=data; }
    catch (_) { Module.hpReplies[id]=JSON.stringify({ok:false,error:{code:'connection',message:'El enlace no tiene un servidor de partidas activo.'}}); }
  }).catch(() => {
    Module.hpReplies[id]=JSON.stringify({ok:false,error:{code:'connection',message:'No pude conectar con la partida. Revisa la conexión y vuelve a intentar.'}});
  }).finally(() => clearTimeout(timeout));
});
EM_JS(char*, PollBrowserReply, (int id), {
  if (!Module.hpReplies || Module.hpReplies[id] === undefined) return 0;
  const result = stringToNewUTF8(Module.hpReplies[id]); delete Module.hpReplies[id]; return result;
});
EM_JS(char*, BrowserRequestId, (), {
  const bytes = new Uint8Array(16); crypto.getRandomValues(bytes);
  return stringToNewUTF8(Array.from(bytes, n => n.toString(16).padStart(2, '0')).join(""));
});
EM_JS(void, SaveBrowserSession, (const char* value), {
  try { if (UTF8ToString(value)) sessionStorage.setItem('hp-session-v2',UTF8ToString(value));
        else sessionStorage.removeItem('hp-session-v2'); } catch (_) {}
});
EM_JS(char*, RestoreBrowserSession, (), {
  try { return stringToNewUTF8(sessionStorage.getItem('hp-session-v2') || ""); } catch (_) { return 0; }
});
#else
#include <curl/curl.h>
#include <chrono>
namespace {
std::size_t AppendBody(char* data, std::size_t size, std::size_t count, void* output) {
  const auto bytes=size*count;
  auto& text=*static_cast<std::string*>(output);
  if (text.size()+bytes>1024*1024) return 0;
  text.append(data,bytes); return bytes;
}
std::string NativeRequest(const std::string& method, const std::string& path, const std::string& body) {
  auto* curl=curl_easy_init();
  if (!curl) return R"({"ok":false,"error":{"code":"connection","message":"No se pudo iniciar la conexión."}})";
  // Native clients use the local classroom server. Other devices use its web URL.
  const char* configured=std::getenv("HP_SERVER_URL");
  const std::string url=std::string(configured?configured:"http://127.0.0.1:8001")+path;
  std::string response;
  curl_easy_setopt(curl,CURLOPT_URL,url.c_str());
  curl_easy_setopt(curl,CURLOPT_TIMEOUT,12L);
  curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,AppendBody);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,&response);
  curl_slist* headers=nullptr;
  if (method=="POST") {
    headers=curl_slist_append(headers,"Content-Type: application/json");
    curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
    curl_easy_setopt(curl,CURLOPT_POSTFIELDS,body.c_str());
  }
  const auto result=curl_easy_perform(curl);
  if (headers) curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  if (result!=CURLE_OK) return R"({"ok":false,"error":{"code":"connection","message":"No pude conectar con el servidor de partidas en localhost:8001."}})";
  return response;
}
}
#endif
namespace hp {
SessionClient::SessionClient() {
#ifndef __EMSCRIPTEN__
  curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
}
SessionClient::~SessionClient() {
#ifndef __EMSCRIPTEN__
  if (response_.valid()) response_.wait();
  curl_global_cleanup();
#endif
}
std::string SessionClient::NewId() {
#ifdef __EMSCRIPTEN__
  char* value=BrowserRequestId();std::string result(value);std::free(value);return result;
#else
  std::random_device source;std::ostringstream out;
  for(int i=0;i<4;++i)out<<std::hex<<std::setw(8)<<std::setfill('0')<<source();
  return out.str();
#endif
}
bool SessionClient::Busy() const { return busy_; }
bool SessionClient::Send(const std::string& method,const std::string& path,const std::string& body) {
  if (busy_) return false;
  busy_=true; ++requestId_;
#ifdef __EMSCRIPTEN__
  SendBrowserRequest(requestId_,method.c_str(),path.c_str(),body.c_str());
#else
  response_=std::async(std::launch::async,NativeRequest,method,path,body);
#endif
  return true;
}
std::optional<std::string> SessionClient::Poll() {
  if (!busy_) return std::nullopt;
#ifdef __EMSCRIPTEN__
  char* value=PollBrowserReply(requestId_);
  if (!value) return std::nullopt;
  std::string result(value); std::free(value);
#else
  if (response_.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return std::nullopt;
  std::string result=response_.get();
#endif
  busy_=false; return result;
}
void SessionClient::SaveSession(const std::string& value) {
#ifdef __EMSCRIPTEN__
  SaveBrowserSession(value.c_str());
#endif
}
std::string SessionClient::RestoreSession() {
#ifdef __EMSCRIPTEN__
  char* value=RestoreBrowserSession(); if (!value) return {};
  std::string result(value);std::free(value);return result;
#else
  return {};
#endif
}
}
