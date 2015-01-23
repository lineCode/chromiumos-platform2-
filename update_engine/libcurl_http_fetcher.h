// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_LIBCURL_HTTP_FETCHER_H_
#define UPDATE_ENGINE_LIBCURL_HTTP_FETCHER_H_

#include <map>
#include <string>
#include <utility>

#include <curl/curl.h>
#include <glib.h>

#include <base/logging.h>
#include <base/macros.h>

#include "update_engine/certificate_checker.h"
#include "update_engine/hardware_interface.h"
#include "update_engine/http_fetcher.h"
#include "update_engine/system_state.h"


// This is a concrete implementation of HttpFetcher that uses libcurl to do the
// http work.

namespace chromeos_update_engine {

class LibcurlHttpFetcher : public HttpFetcher {
 public:
  LibcurlHttpFetcher(ProxyResolver* proxy_resolver,
                     SystemState* system_state)
      : HttpFetcher(proxy_resolver, system_state),
        curl_multi_handle_(nullptr),
        curl_handle_(nullptr),
        curl_http_headers_(nullptr),
        timeout_source_(nullptr),
        transfer_in_progress_(false),
        transfer_size_(0),
        bytes_downloaded_(0),
        download_length_(0),
        resume_offset_(0),
        retry_count_(0),
        max_retry_count_(kDownloadMaxRetryCount),
        retry_seconds_(20),
        no_network_retry_count_(0),
        no_network_max_retries_(0),
        idle_seconds_(1),
        in_write_callback_(false),
        sent_byte_(false),
        terminate_requested_(false),
        check_certificate_(CertificateChecker::kNone),
        low_speed_limit_bps_(kDownloadLowSpeedLimitBps),
        low_speed_time_seconds_(kDownloadLowSpeedTimeSeconds),
        connect_timeout_seconds_(kDownloadConnectTimeoutSeconds) {
    // Dev users want a longer timeout (180 seconds) because they may
    // be waiting on the dev server to build an image.
    if (!system_state->hardware()->IsOfficialBuild())
      low_speed_time_seconds_ = kDownloadDevModeLowSpeedTimeSeconds;
    if (!system_state_->hardware()->IsOOBEComplete(nullptr))
      max_retry_count_ = kDownloadMaxRetryCountOobeNotComplete;
  }

  // Cleans up all internal state. Does not notify delegate
  ~LibcurlHttpFetcher();

  virtual void SetOffset(off_t offset) { bytes_downloaded_ = offset; }

  virtual void SetLength(size_t length) { download_length_ = length; }
  virtual void UnsetLength() { SetLength(0); }

  // Begins the transfer if it hasn't already begun.
  virtual void BeginTransfer(const std::string& url);

  // If the transfer is in progress, aborts the transfer early. The transfer
  // cannot be resumed.
  virtual void TerminateTransfer();

  // Suspend the transfer by calling curl_easy_pause(CURLPAUSE_ALL).
  virtual void Pause();

  // Resume the transfer by calling curl_easy_pause(CURLPAUSE_CONT).
  virtual void Unpause();

  // Libcurl sometimes asks to be called back after some time while
  // leaving that time unspecified. In that case, we pick a reasonable
  // default of one second, but it can be overridden here. This is
  // primarily useful for testing.
  // From http://curl.haxx.se/libcurl/c/curl_multi_timeout.html:
  //     if libcurl returns a -1 timeout here, it just means that libcurl
  //     currently has no stored timeout value. You must not wait too long
  //     (more than a few seconds perhaps) before you call
  //     curl_multi_perform() again.
  void set_idle_seconds(int seconds) { idle_seconds_ = seconds; }

  // Sets the retry timeout. Useful for testing.
  void set_retry_seconds(int seconds) { retry_seconds_ = seconds; }

  void set_no_network_max_retries(int retries) {
    no_network_max_retries_ = retries;
  }

  void set_check_certificate(
      CertificateChecker::ServerToCheck check_certificate) {
    check_certificate_ = check_certificate;
  }

  virtual size_t GetBytesDownloaded() {
    return static_cast<size_t>(bytes_downloaded_);
  }

  virtual void set_low_speed_limit(int low_speed_bps, int low_speed_sec) {
    low_speed_limit_bps_ = low_speed_bps;
    low_speed_time_seconds_ = low_speed_sec;
  }

  virtual void set_connect_timeout(int connect_timeout_seconds) {
    connect_timeout_seconds_ = connect_timeout_seconds;
  }

  virtual void set_max_retry_count(int max_retry_count) {
    max_retry_count_ = max_retry_count;
  }

 private:
  // Callback for when proxy resolution has completed. This begins the
  // transfer.
  void ProxiesResolved();

  // Asks libcurl for the http response code and stores it in the object.
  void GetHttpResponseCode();

  // Checks whether stored HTTP response is within the success range.
  inline bool IsHttpResponseSuccess() {
    return (http_response_code_ >= 200 && http_response_code_ < 300);
  }

  // Checks whether stored HTTP response is within the error range. This
  // includes both errors with the request (4xx) and server errors (5xx).
  inline bool IsHttpResponseError() {
    return (http_response_code_ >= 400 && http_response_code_ < 600);
  }

  // Resumes a transfer where it left off. This will use the
  // HTTP Range: header to make a new connection from where the last
  // left off.
  virtual void ResumeTransfer(const std::string& url);

  // These two methods are for glib main loop callbacks. They are called
  // when either a file descriptor is ready for work or when a timer
  // has fired. The static versions are shims for libcurl which has a C API.
  bool FDCallback(GIOChannel *source, GIOCondition condition);
  static gboolean StaticFDCallback(GIOChannel *source,
                                   GIOCondition condition,
                                   gpointer data) {
    return reinterpret_cast<LibcurlHttpFetcher*>(data)->FDCallback(source,
                                                                   condition);
  }
  gboolean TimeoutCallback();
  static gboolean StaticTimeoutCallback(gpointer data) {
    return reinterpret_cast<LibcurlHttpFetcher*>(data)->TimeoutCallback();
  }

  gboolean RetryTimeoutCallback();
  static gboolean StaticRetryTimeoutCallback(void* arg) {
    return static_cast<LibcurlHttpFetcher*>(arg)->RetryTimeoutCallback();
  }

  // Calls into curl_multi_perform to let libcurl do its work. Returns after
  // curl_multi_perform is finished, which may actually be after more than
  // one call to curl_multi_perform. This method will set up the glib run
  // loop with sources for future work that libcurl will do.
  // This method will not block.
  // Returns true if we should resume immediately after this call.
  void CurlPerformOnce();

  // Sets up glib main loop sources as needed by libcurl. This is generally
  // the file descriptor of the socket and a timer in case nothing happens
  // on the fds.
  void SetupMainloopSources();

  // Callback called by libcurl when new data has arrived on the transfer
  size_t LibcurlWrite(void *ptr, size_t size, size_t nmemb);
  static size_t StaticLibcurlWrite(void *ptr, size_t size,
                                   size_t nmemb, void *stream) {
    return reinterpret_cast<LibcurlHttpFetcher*>(stream)->
        LibcurlWrite(ptr, size, nmemb);
  }

  // Cleans up the following if they are non-null:
  // curl(m) handles, io_channels_, timeout_source_.
  void CleanUp();

  // Force terminate the transfer. This will invoke the delegate's (if any)
  // TransferTerminated callback so, after returning, this fetcher instance may
  // be destroyed.
  void ForceTransferTermination();

  // Sets the curl options for HTTP URL.
  void SetCurlOptionsForHttp();

  // Sets the curl options for HTTPS URL.
  void SetCurlOptionsForHttps();

  // Convert a proxy URL into a curl proxy type, if applicable. Returns true iff
  // conversion was successful, false otherwise (in which case nothing is
  // written to |out_type|).
  bool GetProxyType(const std::string& proxy, curl_proxytype* out_type);

  // Handles for the libcurl library
  CURLM *curl_multi_handle_;
  CURL *curl_handle_;
  struct curl_slist *curl_http_headers_;

  // Lists of all read(0)/write(1) file descriptors that we're waiting on from
  // the glib main loop. libcurl may open/close descriptors and switch their
  // directions so maintain two separate lists so that watch conditions can be
  // set appropriately.
  typedef std::map<int, std::pair<GIOChannel*, guint>> IOChannels;
  IOChannels io_channels_[2];

  // if non-null, a timer we're waiting on. glib main loop will call us back
  // when it fires.
  GSource* timeout_source_;

  bool transfer_in_progress_;

  // The transfer size. -1 if not known.
  off_t transfer_size_;

  // How many bytes have been downloaded and sent to the delegate.
  off_t bytes_downloaded_;

  // The remaining maximum number of bytes to download. Zero represents an
  // unspecified length.
  size_t download_length_;

  // If we resumed an earlier transfer, data offset that we used for the
  // new connection.  0 otherwise.
  // In this class, resume refers to resuming a dropped HTTP connection,
  // not to resuming an interrupted download.
  off_t resume_offset_;

  // Number of resumes performed so far and the max allowed.
  int retry_count_;
  int max_retry_count_;

  // Seconds to wait before retrying a resume.
  int retry_seconds_;

  // Number of resumes due to no network (e.g., HTTP response code 0).
  int no_network_retry_count_;
  int no_network_max_retries_;

  // Seconds to wait before asking libcurl to "perform".
  int idle_seconds_;

  // If true, we are currently performing a write callback on the delegate.
  bool in_write_callback_;

  // If true, we have returned at least one byte in the write callback
  // to the delegate.
  bool sent_byte_;

  // We can't clean everything up while we're in a write callback, so
  // if we get a terminate request, queue it until we can handle it.
  bool terminate_requested_;

  // Represents which server certificate to be checked against this
  // connection's certificate. If no certificate check needs to be performed,
  // this should be kNone.
  CertificateChecker::ServerToCheck check_certificate_;

  int low_speed_limit_bps_;
  int low_speed_time_seconds_;
  int connect_timeout_seconds_;
  int num_max_retries_;

  DISALLOW_COPY_AND_ASSIGN(LibcurlHttpFetcher);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_LIBCURL_HTTP_FETCHER_H_