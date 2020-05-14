#ifndef REQUESTS_HPP
#define REQUESTS_HPP

#include <curl/curl.h>
#include <memory>
#include <string>
#include <map>


namespace requests {
  using namespace std;

  typedef struct {
    char *data;
    size_t size;
  } Buffer;

  static size_t write_cb(void *data, size_t size, size_t nmemb, void *user_data) {
    size_t real_size = size * nmemb;
    auto *buf = (Buffer *) user_data;

    auto *ptr = (char *) realloc(buf->data, buf->size + real_size + 1);
    if (ptr == nullptr)
      return 0;  /* out of memory! */

    buf->data = ptr;
    memcpy(&(buf->data[buf->size]), data, real_size);
    buf->size += real_size;
    buf->data[buf->size] = 0;

    return real_size;
  }

  static unsigned char to_hex(unsigned char x) {
    return x > 9 ? x + 55 : x + 48;
  }

  static unsigned char from_hex(unsigned char x) {
    unsigned char y;
    if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
    else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
    else if (x >= '0' && x <= '9') y = x - '0';
    else
      assert(0);
    return y;
  }

  string url_encode(const string &str) {
    std::string strTemp;
    size_t length = str.length();
    for (size_t i = 0; i < length; i++) {
      if (isalnum((unsigned char) str[i]) ||
          (str[i] == ':') ||
          (str[i] == '/') ||
          (str[i] == '?') ||
          (str[i] == '=') ||
          (str[i] == '-') ||
          (str[i] == '_') ||
          (str[i] == '.') ||
          (str[i] == '~'))
        strTemp += str[i];
      else if (str[i] == ' ')
        strTemp += "+";
      else {
        strTemp += '%';
        strTemp += to_hex((unsigned char) str[i] >> 4);
        strTemp += to_hex((unsigned char) str[i] % 16);
      }
    }
    return strTemp;
  }

  string url_decode(const string &str) {
    std::string strTemp;
    size_t length = str.length();
    for (size_t i = 0; i < length; i++) {
      if (str[i] == '+') strTemp += ' ';
      else if (str[i] == '%') {
        assert(i + 2 < length);
        unsigned char high = from_hex((unsigned char) str[++i]);
        unsigned char low = from_hex((unsigned char) str[++i]);
        strTemp += high * 16 + low;
      } else strTemp += str[i];
    }
    return strTemp;
  }

  enum HTTPMethod {
    GET, POST, PUT, DELETE, HEAD, TRACE, OPTIONS, LOCK, MKCOL, COPY, MOVE
  };

  class Request {
  public:
    Request(HTTPMethod method, const string &url, const map<string, string> &headers, const string &cookie)
      : method(method), url(url), headers(headers), cookie(cookie) {}

  public:
    HTTPMethod method;
    const string &url;
    const map<string, string> &headers;
    const string &cookie;
    const map<string, string> files;
    void *data;
    const map<string, string> params;
    void *auth;
  };

  class Response {
  public:
    explicit Response(const string &url, int status_code = -1)
      : url(url), status_code(status_code), elapsed(0) {}

    string text() { return content; }

  public:
    const string &url;
    int status_code;
    string reason;
    string content;
    double elapsed;
  };

  Response get(const string &url, const map<string, string> &headers, const string &cookie) {
    Response resp(url);

    CURL *curl = curl_easy_init();
    if (curl == nullptr) {
      resp.reason = "failed initialize curl.";
      return resp;
    }

    Buffer chunk = {};
    curl_easy_setopt(curl, CURLOPT_URL, url_encode(url).c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &chunk);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, false);

    struct curl_slist *headers_ls = nullptr;
    for (auto &iter:headers) {
      string item = iter.first + ":" + iter.second;
      headers_ls = curl_slist_append(headers_ls, item.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers_ls);

    if (!cookie.empty())
      curl_easy_setopt(curl, CURLOPT_COOKIELIST, cookie.c_str());

    CURLcode code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status_code);
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &resp.elapsed);

    if (code != CURLE_OK) resp.reason = curl_easy_strerror(code);
    if (chunk.data) {
      resp.content = string(chunk.data);
      free(chunk.data);
    }

    curl_easy_cleanup(curl);
    return resp;
  }

  Response get(const char *url) {
    return get(url, map<string, string>(), string());
  }
}

#endif //REQUESTS_HPP

