#include "html_parser.h"
#include <sstream>
#include <iterator>

html_parser::html_parser(const char* buffer, int buffer_length) : request_type(-1) {
    if(buffer_length == 0){
        return;
    }
    std::vector<std::string> lines;
    std::istringstream stream(std::string(buffer, buffer_length));
    std::string line;

    while (std::getline(stream, line)) {
        lines.push_back(line);
    }

    std::istringstream first_line(lines[0]);
    std::vector<std::string> tokens{std::istream_iterator<std::string>{first_line}, std::istream_iterator<std::string>{}};

    if(!tokens.empty()){
        if (tokens[0] == "GET"){
            request_type = 0;
        } else if (tokens[0] == "POST"){
            request_type = 1;
        } else if (tokens[0] == "PUT"){
            request_type = 2;
        }

        url = tokens.size() > 1 ? tokens[1] : "";

        if (request_type == 0) {
            url_parser(url);
        } else {
            text = lines.back();
        }
    }
}

int html_parser::get_request_type() const {
    return request_type;
}

void html_parser::url_parser(const std::string& url) {
    auto pos = url.find("name=");
    if (pos != std::string::npos) {
        request_inputs["name"] = url.substr(pos + 5);
    }

}

std::string html_parser::get_input(const std::string& key) const {
    auto it = request_inputs.find(key);
    return it != request_inputs.end() ? it->second : "";
}

std::string html_parser::get_text() const {
    return text;
}