// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_URL_UTILS_H_
#define LIBCHROMEOS_CHROMEOS_URL_UTILS_H_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <chromeos/chromeos_export.h>
#include <chromeos/data_encoding.h>

namespace chromeos {

namespace url {

// Appends a subpath to url and delimiting then with '/' if the path doesn't
// end with it already. Also handles URLs with query parameters/fragment.
CHROMEOS_EXPORT std::string Combine(
    const std::string& url,
    const std::string& subpath) WARN_UNUSED_RESULT;
CHROMEOS_EXPORT std::string CombineMultiple(
    const std::string& url,
    const std::vector<std::string>& parts) WARN_UNUSED_RESULT;

// Removes the query string/fragment from |url| and returns the query string.
// This method actually modifies |url|. So, if you call it on this:
//    http://www.test.org/?foo=bar
// it will modify |url| to "http://www.test.org/" and return "?foo=bar"
CHROMEOS_EXPORT std::string TrimOffQueryString(std::string* url);

// Returns the query string, if available.
// For example, for the following URL:
//    http://server.com/path/to/object?k=v&foo=bar#fragment
// Here:
//    http://server.com/path/to/object - is the URL of the object,
//    ?k=v&foo=bar                     - URL query string
//    #fragment                        - URL fragment string
// If |remove_fragment| is true, the function returns the query string without
// the fragment. Otherwise the fragment is included as part of the result.
CHROMEOS_EXPORT std::string GetQueryString(const std::string& url,
                                           bool remove_fragment);

// Parses the query string into a set of key-value pairs.
CHROMEOS_EXPORT data_encoding::WebParamList GetQueryStringParameters(
    const std::string& url);

// Returns a value of the specified query parameter, or empty string if missing.
CHROMEOS_EXPORT std::string GetQueryStringValue(
    const std::string& url,
    const std::string& name);
CHROMEOS_EXPORT std::string GetQueryStringValue(
    const data_encoding::WebParamList& params,
    const std::string& name);

// Removes the query string and/or a fragment part from URL.
// If |remove_fragment| is specified, the fragment is also removed.
// For example:
//    http://server.com/path/to/object?k=v&foo=bar#fragment
// true  -> http://server.com/path/to/object
// false -> http://server.com/path/to/object#fragment
CHROMEOS_EXPORT std::string RemoveQueryString(
    const std::string& url,
    bool remove_fragment) WARN_UNUSED_RESULT;

// Appends a single query parameter to the URL.
CHROMEOS_EXPORT std::string AppendQueryParam(
    const std::string& url,
    const std::string& name,
    const std::string& value) WARN_UNUSED_RESULT;
// Appends a list of query parameters to the URL.
CHROMEOS_EXPORT std::string AppendQueryParams(
    const std::string& url,
    const data_encoding::WebParamList& params) WARN_UNUSED_RESULT;

// Checks if the URL has query parameters.
CHROMEOS_EXPORT bool HasQueryString(const std::string& url);

}  // namespace url
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_URL_UTILS_H_
