#pragma once

#include <string>
#include <map>
#include <vector>

class html_parser {
public:
    html_parser(const char* buffer, int buffer_length);
    int get_request_type() const;
    std::string get_input(const std::string& key) const;
    std::string get_text() const;

private:
    void url_parser(const std::string& url);

    int request_type;
    std::string url;
    std::map<std::string, std::string> request_inputs;
    std::string text;
};

