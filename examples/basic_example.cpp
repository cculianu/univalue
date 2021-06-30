// A simple basic example of how to use the UniValye library.
#include "univalue.h"

#include <cassert>
#include <iostream>
#include <string>

namespace {

void parseAndProcessObjectExample()
{
    // print banner
    std::cout << std::string(79, '-') << std::endl;
    std::cout << __func__ << std::endl;
    std::cout << std::string(79, '-') << std::endl;


    // An example of how to parse an object and examine it

    const std::string json{
        "{"
        "    \"this is a JSON object\": \"it's pretty neat\" ,"
        "    \"akey\": 3.14,"
        "    \"theanswer\": 42,"
        "    \"thequestion\": false,"
        "    \"alist\": [1,2,3,4,\"hahaha\"]"
        "}"
    };
    UniValue uv;
    const bool ok [[maybe_unused]] = uv.read(json);
    assert(ok); // parse of valid json
    assert(uv.isObject()); // uv.isObject() is true
    const auto &obj = uv.get_obj(); // this would throw std::runtime_error if !uv.isObject()
    for (const auto & [key, value] : obj) {
        if (key == "theanswer")
            std::cout << "the answer is: " << value.get_int64() << std::endl; // throws if the value is not numeric
        else if (key == "thequestion")
            std::cout << "the question is: " << value.get_bool() << std::endl; // throws if value is not boolean
        else if (key == "alist" && value.isArray()) {
            std::cout << "the list: " << std::flush;
            int i = 0;
            for (const auto & item : value.get_array())
                std::cout << (i++ ? ", " : "") << item.getValStr(); // getValStr() returns the contents of either a numeric or a string
            std::cout << std::endl;
        }
    }
    /*
       Program output for above is:

       the answer is: 42
       the question is: 0
       the list: 1, 2, 3, 4, hahaha
    */
}

void buildObjectExample()
{
    // print banner
    std::cout << std::string(79, '-') << std::endl;
    std::cout << __func__ << std::endl;
    std::cout << std::string(79, '-') << std::endl;


    // An example of how to build an object

    UniValue uv;
    auto &obj = uv.setObject(); // this clears the uv instance and sets it to type VOBJ, returning a reference to the underlying Object
    obj.emplace_back("this is a JSON object", "it's pretty neat");
    obj.emplace_back("akey", 3.14);
    obj.emplace_back("theanswer", 42);
    obj.emplace_back("thequestion", false);
    obj.emplace_back("alist", UniValue::Array{{ 1, 2, 3, 4, "hahaha" }});

    // the below stringifies or serializes the constructed object
    std::cout << UniValue::stringify(uv, 4 /* pretty indent 4 spaces */) << std::endl;

    /*
       Program output for above is:
       {
           "this is a JSON object": "it's pretty neat",
           "akey": 3.14,
           "theanswer": 42,
           "thequestion": false,
           "alist": [
               1,
               2,
               3,
               4,
               "hahaha"
           ]
       }
    */
}

} // namespace

int main()
{
    buildObjectExample();
    parseAndProcessObjectExample();
    return 0;
}

