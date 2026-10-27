#pragma once
#include <future>
#include <optional>
#include <string>

namespace hp {
// One request in flight. All graphics and state application remain on the main
// thread; the native HTTP operation runs in a worker, browser uses fetch.
class SessionClient {
 public:
  SessionClient();
  ~SessionClient();
  static std::string NewId();
  bool Busy() const;
  bool Send(const std::string& method, const std::string& path,
            const std::string& body = "");
  std::optional<std::string> Poll();
  void SaveSession(const std::string& value);
  std::string RestoreSession();
 private:
  int requestId_ = 0;
  bool busy_ = false;
#ifndef __EMSCRIPTEN__
  std::future<std::string> response_;
#endif
};
}
